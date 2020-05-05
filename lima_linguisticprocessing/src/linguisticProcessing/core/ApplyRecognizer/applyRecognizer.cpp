/*
    Copyright 2002-2020 CEA LIST

    This file is part of LIMA.

    LIMA is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    LIMA is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with LIMA.  If not, see <http://www.gnu.org/licenses/>
*/
/************************************************************************
 *
 * @file       applyRecognizer.cpp
 * @author     besancon (besanconr@zoe.cea.fr)
 * @date       Fri Jan 14 2005
 * @version    $Id$
 * copyright   Copyright (C) 2005-2020 by CEA LIST
 *
 ***********************************************************************/

#include "applyRecognizer.h"
#include "linguisticProcessing/core/Automaton/recognizerData.h"

#include "common/AbstractFactoryPattern/SimpleFactory.h"
#include "linguisticProcessing/common/annotationGraph/AnnotationData.h"
#include "common/XMLConfigurationFiles/xmlConfigurationFileExceptions.h"
#include "common/time/timeUtilsController.h"
#include "linguisticProcessing/core/LinguisticResources/AbstractResource.h"
#include "linguisticProcessing/core/LinguisticResources/LinguisticResources.h"

using namespace Lima::Common::AnnotationGraphs;
using namespace Lima::LinguisticProcessing::LinguisticAnalysisStructure;
using namespace Lima::LinguisticProcessing::Automaton;
using namespace std;

namespace Lima {
namespace LinguisticProcessing {
namespace ApplyRecognizer {

SimpleFactory<MediaProcessUnit,ApplyRecognizer> ApplyRecognizer(APPLYRECOGNIZER_CLASSID);

#define LOG_ERROR_AND_THROW(msg, exc) { \
                                        APPRLOGINIT; \
                                        LERROR << msg; \
                                        throw exc; \
                                      }

ApplyRecognizer::ApplyRecognizer():
MediaProcessUnit(),
m_recognizers(0),
m_useSentenceBounds(false),
m_updateGraph(false),
m_resolveOverlappingEntities(false),
m_overlappingEntitiesStrategy(IGNORE_SMALLEST),
m_testAllVertices(false),
m_stopAtFirstSuccess(true),
m_onlyOneSuccessPerType(false),
m_graphId("PosGraph"),
m_dataForStorage()
{
}

ApplyRecognizer::~ApplyRecognizer()
{
}

void ApplyRecognizer::init(
  Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration,
  Manager* manager)

{
  APPRLOGINIT;
  MediaId language=manager->getInitializationParameters().media;
  try {
    // try to get a single automaton
    string automaton=unitConfiguration.getParamsValueAtKey("automaton");
    AbstractResource* res=LinguisticResources::single().getResource(language,automaton);
    m_recognizers.push_back(static_cast<Recognizer*>(res));
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    try {
    // try to get a list of automatons
      const deque<string>& automatonList=unitConfiguration.getListsValueAtKey("automatonList");
      for (deque<string>::const_iterator it=automatonList.begin(),
             it_end=automatonList.end(); it!=it_end; it++) {
        AbstractResource* res=LinguisticResources::single().getResource(language,*it);
        m_recognizers.push_back(static_cast<Recognizer*>(res));
      }
    }
    catch (Common::XMLConfigurationFiles::NoSuchList& ) {
      LERROR << "No 'automaton' or 'automatonList' in ApplyRecognizer group for language "
             << (int)language << " !";
      throw InvalidConfiguration();
    }
  }

  try {
    m_useSentenceBounds=
      getBooleanParameter(unitConfiguration,"useSentenceBounds");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    m_updateGraph=
      getBooleanParameter(unitConfiguration,"updateGraph");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    m_resolveOverlappingEntities=
      getBooleanParameter(unitConfiguration,"resolveOverlappingEntities");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    string overlappingEntitiesStrategy=
      unitConfiguration.getParamsValueAtKey("overlappingEntitiesStrategy");
    if (overlappingEntitiesStrategy=="IgnoreSmallest") {
      m_overlappingEntitiesStrategy=IGNORE_SMALLEST;
    }
    else if (overlappingEntitiesStrategy=="IgnoreFirst") {
      m_overlappingEntitiesStrategy=IGNORE_FIRST;
    }
    else if (overlappingEntitiesStrategy=="IgnoreSecond") {
      m_overlappingEntitiesStrategy=IGNORE_SECOND;
    }
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    m_testAllVertices=
      getBooleanParameter(unitConfiguration,"testAllVertices");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    m_stopAtFirstSuccess=
      getBooleanParameter(unitConfiguration,"stopAtFirstSuccess");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }


  try {
    m_onlyOneSuccessPerType=
      getBooleanParameter(unitConfiguration,"onlyOneSuccessPerType");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try
  {
    m_graphId=unitConfiguration.getParamsValueAtKey("applyOnGraph");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& )
  {
    // optional parameter: keep default value
  }

  try {
    m_dataForStorage=unitConfiguration.getParamsValueAtKey("storeInData");
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

}

bool ApplyRecognizer::
getBooleanParameter(Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration,
                    const std::string& param) const {
  string value=unitConfiguration.getParamsValueAtKey(param);
  if (value == "yes" ||
      value == "true" ||
      value == "1") {
    return true;
  }
  return false;
}

LimaStatusCode ApplyRecognizer::process(AnalysisContent& analysis) const
{
  Lima::TimeUtilsController timer("ApplyRecognizer");
  if (m_recognizers.empty()) {
    APPRLOGINIT;
    LDEBUG << "ApplyRecognizer: No recognizer to apply";
    return MISSING_DATA;
  }
#ifdef DEBUG_LP
  APPRLOGINIT;
  LINFO << "start process";
  LDEBUG << "  parameters are:";
  LDEBUG << "    - useSentenceBounds           :" << m_useSentenceBounds;
  LDEBUG << "    - updateGraph                 :" << m_updateGraph;
  LDEBUG << "    - resolveOverlappingEntities  :" << m_resolveOverlappingEntities;
  LDEBUG << "    - overlappingEntitiesStrategy :" << m_overlappingEntitiesStrategy;
  LDEBUG << "    - testAllVertices             :" << m_testAllVertices;
  LDEBUG << "    - stopAtFirstSuccess          :" << m_stopAtFirstSuccess;
  LDEBUG << "    - onlyOneSuccessPerType       :" << m_onlyOneSuccessPerType;
  LDEBUG << "    - graphId                     :" << m_graphId;
  LDEBUG << "    - dataForStorage              :" << m_dataForStorage;
#endif

  LimaStatusCode returnCode(SUCCESS_ID);

  RecognizerData* recoData=static_cast<RecognizerData*>(analysis.getData("RecognizerData"));
  if (recoData == 0)
  {
    recoData = new RecognizerData();
    analysis.setData("RecognizerData", recoData);
  }

  AnnotationData* annotationData = static_cast< AnnotationData* >(analysis.getData("AnnotationData"));
  if (annotationData==0)
  {
    annotationData=new AnnotationData();
    if (static_cast<AnalysisGraph*>(analysis.getData("AnalysisGraph")) != 0)
    {
      static_cast<AnalysisGraph*>(analysis.getData("AnalysisGraph"))->populateAnnotationGraph(annotationData, "AnalysisGraph");
    }
    analysis.setData("AnnotationData",annotationData);
  }

  // data to possibly store the result (according to the actions)
  // assume all recognizers use the same entity type group (gloups)
  RecognizerResultData* resultData=
    new RecognizerResultData(m_graphId);
  recoData->setResultData(resultData);

  if (m_useSentenceBounds) {
    for (vector<Recognizer*>::const_iterator reco=m_recognizers.begin(),
           reco_end=m_recognizers.end();reco!=reco_end; reco++) {
      returnCode=processOnEachSentence(analysis,*reco,recoData);
    }
  }
  else {
    for (vector<Recognizer*>::const_iterator reco=m_recognizers.begin(),
           reco_end=m_recognizers.end();reco!=reco_end; reco++) {
      returnCode=processOnWholeText(analysis,*reco,recoData);
    }
  }

  if (m_updateGraph) {
//     LDEBUG << "";
    recoData->removeVertices(analysis);
    recoData->clearVerticesToRemove();
    recoData->removeEdges(analysis);
    recoData->clearEdgesToRemove();
  }

  if (! m_dataForStorage.empty()) {
    analysis.setData(m_dataForStorage,resultData);
  }
  else {
    // result data stored in recoData and resultData are same pointer
    recoData->deleteResultData();
    resultData=0;
  }

  // remove recognizer data (used only internally to this process unit)
  analysis.removeData("RecognizerData");

  return returnCode;
}

bool ApplyRecognizer::checkGraphConsistency(const SegmentationData* sb,
                                            const AnalysisGraph& graph) const
{
  APPRLOGINIT
  std::vector<Segment>::const_iterator boundItr=(sb->getSegments()).begin();
  while (boundItr!=(sb->getSegments()).end())
  {
    LinguisticGraphVertex endSentence=boundItr->getLastVertex();

    LinguisticGraphOutEdgeIt outItr,outItrEnd;
    size_t counter=0;
    for (boost::tie(outItr,outItrEnd)=boost::out_edges(endSentence,*graph.getGraph());
         outItr!=outItrEnd; outItr++)
    {
      counter+=1;
    }
    if (0==counter)
    {
      LERROR << "ERROR: no way from " << endSentence;
      return false;
    }
    boundItr++;
  }
  return true;
}

LimaStatusCode ApplyRecognizer::
processOnEachSentence(AnalysisContent& analysis,
                      Recognizer* reco,
                      RecognizerData* recoData) const
{
  APPRLOGINIT;

  AnalysisGraph* anagraph = static_cast<AnalysisGraph*>(analysis.getData(recoData->getGraphId()));
  if (nullptr==anagraph) {
    LERROR << "graph with id '"<< recoData->getGraphId() <<"' is not available";
    return MISSING_DATA;
  }

  // get sentence bounds
  SegmentationData* sb=static_cast<SegmentationData*>(analysis.getData("SentenceBoundaries"));
  if (sb==0)
  {
    LERROR << "no sentence bounds defined ! abort";
    return MISSING_DATA;
  }

  std::vector<RecognizerMatch> seRecognizerResult;
  std::vector<Segment>::iterator boundItr=(sb->getSegments()).begin();
  while (boundItr!=(sb->getSegments()).end())
  {
    LinguisticGraphVertex beginSentence=boundItr->getFirstVertex();
    LinguisticGraphVertex endSentence=boundItr->getLastVertex();

    LDEBUG << "analyze sentence from vertex " << beginSentence << " to vertex " << endSentence;

    SetOfLinguisticGraphVertices afterEnd=getFollowingNodes<SetOfLinguisticGraphVertices>(*anagraph,endSentence);

    if (0==afterEnd.size())
      LOG_ERROR_AND_THROW("\"" << endSentence << "\" is the last vertex in the graph.", LimaException());

    seRecognizerResult.clear();
    reco->apply(*anagraph,beginSentence,endSentence,analysis,seRecognizerResult);

    SetOfLinguisticGraphVertices newEndNodes;
    for (auto v : afterEnd)
    {
      for (auto z : getPrecedingNodes<SetOfLinguisticGraphVertices>(*anagraph,v))
      {
        newEndNodes.insert(z);
      }
    }

    if (1 != newEndNodes.size())
    {
      LOG_ERROR_AND_THROW("There is " << newEndNodes.size() << " vericies in the end of sentence.",
                          LimaException());
      // Last token must have only one corresponding vertex.
    }

    LinguisticGraphVertex newVertex = *newEndNodes.cbegin();

    if (newVertex!=endSentence)
    {
      boundItr->setLastVertex(newVertex);
      std::vector<Segment>::iterator nextSegmentItr=boundItr;
      nextSegmentItr++;
      nextSegmentItr->setFirstVertex(newVertex);
    }

    //remove overlapping entities
    if (m_resolveOverlappingEntities) {
      reco->resolveOverlappingEntities(seRecognizerResult,
                                       m_overlappingEntitiesStrategy);
    }

    boundItr++;
    recoData->nextSentence();
  }

  return SUCCESS_ID;
}

LimaStatusCode ApplyRecognizer::
processOnWholeText(AnalysisContent& analysis,
                   Recognizer* reco,
                   RecognizerData* recoData ) const
{
//   APPRLOGINIT;
//   LDEBUG << "apply recognizer on whole text";

  AnalysisGraph* anagraph=static_cast<AnalysisGraph*>(analysis.getData(recoData->getGraphId()));
  if (nullptr==anagraph) {
    APPRLOGINIT;
    LERROR << "graph with id '" << recoData->getGraphId() << "' is not available";
    return MISSING_DATA;
  }

  std::vector<RecognizerMatch> seRecognizerResult;

  map<LinguisticGraphVertex,size_t> oldFirstVertices;
  SegmentationData* sb=static_cast<SegmentationData*>(analysis.getData("SentenceBoundaries"));
  if (nullptr!=sb)
  {
    for (size_t i=0;i<sb->getSegments().size();i++)
    {
      for (auto v : getFollowingNodes<SetOfLinguisticGraphVertices>(*anagraph,sb->getSegments()[i].getFirstVertex()))
      {
        oldFirstVertices[v]=i;
      }
    }
  }

  reco->apply(*anagraph,
              anagraph->firstVertex(),
              anagraph->lastVertex(),
              analysis,seRecognizerResult,
              m_testAllVertices,m_stopAtFirstSuccess,m_onlyOneSuccessPerType);

  // The application of rules replaces the old vertices with the new ones.
  // SentenceBoundaries holds the identifiers of verticies. Here we check that
  // the boundaries are still valid. In case the boundaries aren't valid
  // the corresponding segments have to be merged.
  if (nullptr!=sb)
  {
    updateSegmentation(*sb,*anagraph,oldFirstVertices);
  }

  //remove overlapping entities
  if (m_resolveOverlappingEntities) {
    reco->resolveOverlappingEntities(seRecognizerResult,
                                     m_overlappingEntitiesStrategy);
  }

  return SUCCESS_ID;
}

size_t ApplyRecognizer::
updateSegmentation(SegmentationData& sb,
                   const AnalysisGraph& graph,
                   const map<LinguisticGraphVertex, size_t>& oldFirstVertices) const
{
  APPRLOGINIT;
  size_t segmentsMerged = 0;

  set<LinguisticGraphVertex> lastVerticies;
  auto boundItr=sb.getSegments().begin();
  while (boundItr!=sb.getSegments().end())
  {
    lastVerticies.insert(boundItr->getLastVertex());
    boundItr++;
  }

  boundItr=sb.getSegments().begin();
  set<LinguisticGraphVertex> visited;

  size_t i=0;
  while (i<sb.getSegments().size())
  {
    list<LinguisticGraphVertex> toVisit;
    toVisit.push_back(sb.getSegments()[i].getFirstVertex());

    while (!toVisit.empty())
    {
      LinguisticGraphVertex currentVertex=toVisit.front();
      toVisit.pop_front();
      visited.insert(currentVertex);

      auto it=oldFirstVertices.find(currentVertex);
      if (oldFirstVertices.end()!=it)
      {
        size_t segment_no=it->second-segmentsMerged;
        bool beginFound=false;
        set<LinguisticGraphVertex> preceding_vertices=getPrecedingNodes<SetOfLinguisticGraphVertices>(graph,currentVertex);
        for ( LinguisticGraphVertex v : preceding_vertices )
        {
          if (sb.getSegments()[segment_no].getFirstVertex() == v)
          {
            beginFound=true;
            break;
          }
        }
        if (!beginFound)
        {
          if (1 != preceding_vertices.size())
          {
            LOG_ERROR_AND_THROW("There is " << preceding_vertices.size() << " vericies in the end of sentence.",
                          LimaException());
          }

          sb.getSegments()[segment_no].setFirstVertex(*preceding_vertices.begin());
          if (segment_no>0)
          {
            sb.getSegments()[segment_no-1].setLastVertex(*preceding_vertices.begin());
          }
          i++;
        }
      }

      if (lastVerticies.end()!=lastVerticies.find(currentVertex))
      {
        if (sb.getSegments()[i].getLastVertex()==currentVertex)
        {
          lastVerticies.erase(lastVerticies.find(currentVertex));
          break;
        }

        // we reached the end of some segment
        size_t f=i;
        i++;

        if (i>=sb.getSegments().size())
        {
          break;
        }

        while (i<sb.getSegments().size() && sb.getSegments()[i].getLastVertex()!=currentVertex)
        {
          i++;
        }

        if (i>=sb.getSegments().size())
        {
          break;
        }

        sb.getSegments()[f].setLastVertex(sb.getSegments()[i].getLastVertex());

        uint64_t len = sb.getSegments()[i].getLength();
        for (size_t j=f; j<i; j++)
        {
          len += sb.getSegments()[j].getLength();
        }
        sb.getSegments()[f].setLength(len);

        f++;
        sb.getSegments().erase(sb.getSegments().begin()+f, sb.getSegments().begin()+i+1);
        LDEBUG << "Sentences [" << f << ", " << i+1 << ") removed.";
        i=f-1;
        segmentsMerged++;
        lastVerticies.erase(lastVerticies.find(currentVertex));
        break;
      }

      for ( LinguisticGraphVertex next : getFollowingNodes<SetOfLinguisticGraphVertices>(graph, currentVertex) )
      {
        if (visited.end()==visited.find(next))
        {
          toVisit.push_back(next);
        }
      }
    }

    i++;
  }

  LDEBUG << "segmentsMerged==" << segmentsMerged;
  return segmentsMerged;
}

} // end namespace
} // end namespace
} // end namespace
