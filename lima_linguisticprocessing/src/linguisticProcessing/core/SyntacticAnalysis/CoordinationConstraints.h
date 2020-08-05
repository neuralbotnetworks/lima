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
/**
  * @brief       this file contains the definitions of several constraint
  *              functions for the detection of subsentences
  *
  * @file        SimplificationConstraints.h
  * @author      Gael de Chalendar (Gael.de-Chalendar@cea.fr)

  *              Copyright (c) 2005 by CEA
  * @date        Created on Tue Mar, 15 2005
  *
  *
  */

#ifndef LIMA_SYNTACTICANALYSIS_SIMPLIFICATIONCONSTRAINTS_H
#define LIMA_SYNTACTICANALYSIS_SIMPLIFICATIONCONSTRAINTS_H

#include "SyntacticAnalysisExport.h"
#include "HomoSyntagmaticConstraints.h"
#include "linguisticProcessing/core/Automaton/constraintFunction.h"
#include "linguisticProcessing/core/Automaton/recognizerMatch.h"

#include <iostream>

namespace Lima {
namespace LinguisticProcessing {
namespace SyntacticAnalysis {

//**********************************************************************
// ids of constraints defined in this file
#define DefineStringId "DefineString"
#define SameStringId "SameString"
#define DefineModelId "DefineModel"
#define SetInstanceId "SetInstance"

//**********************************************************************
class LIMA_SYNTACTICANALYSIS_EXPORT DefineString : public Automaton::ConstraintFunction
{
public:
  explicit DefineString(MediaId language,
             const LimaString& complement=LimaString());
  ~DefineString() {}

  bool operator()(const Lima::LinguisticProcessing::LinguisticAnalysisStructure::AnalysisGraph& graph,
                  const LinguisticGraphVertex& v1,
                  const LinguisticGraphVertex& v2,
                  AnalysisContent& analysis) const override;
private:
  uint64_t m_language;
};



class LIMA_SYNTACTICANALYSIS_EXPORT SameString : public Automaton::ConstraintFunction
{
public:
  explicit SameString(MediaId language,
                    const LimaString& complement=LimaString());
  ~SameString() {}

  bool operator()(const Lima::LinguisticProcessing::LinguisticAnalysisStructure::AnalysisGraph& graph,
                  const LinguisticGraphVertex& v1,
                  const LinguisticGraphVertex& v2,
                  AnalysisContent& analysis) const override;

  bool actionNeedsRecognizedExpression() override { return true; }

private:
  uint64_t m_language;
  const Common::PropertyCode::PropertyAccessor* m_microAccessor;
};

class LIMA_SYNTACTICANALYSIS_EXPORT DefineModel : public Automaton::ConstraintFunction
{
public:
  explicit DefineModel(MediaId language,
           const LimaString& complement=LimaString());
  ~DefineModel() {}

  bool operator()(const Lima::LinguisticProcessing::LinguisticAnalysisStructure::AnalysisGraph& graph,
                  const LinguisticGraphVertex& v1,
                  const LinguisticGraphVertex& v2,
                  AnalysisContent& analysis) const override;

};

class LIMA_SYNTACTICANALYSIS_EXPORT SetInstance : public Automaton::ConstraintFunction
{
public:
  explicit SetInstance(MediaId language,
                        const LimaString& complement=LimaString());
  ~SetInstance() {}

  bool operator()(
          const Lima::LinguisticProcessing::LinguisticAnalysisStructure::AnalysisGraph& graph,
          const LinguisticGraphVertex& v1,
          const LinguisticGraphVertex& v2,
          AnalysisContent& analysis) const override;
};


} // end namespace SyntacticAnalysis
} // end namespace LinguisticProcessing
} // end namespace Lima

#endif // LIMA_SYNTACTICANALYSIS_SIMPLIFICATIONCONSTRAINTS_H
