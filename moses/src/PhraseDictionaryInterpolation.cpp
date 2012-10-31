/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

//#include "PhraseDictionaryMultiModel.h"
#include "PhraseDictionaryInterpolation.h"

using namespace std;

namespace Moses

{
PhraseDictionaryInterpolation::PhraseDictionaryInterpolation(size_t numScoreComponent,
    PhraseDictionaryFeature* feature): PhraseDictionaryMultiModel(numScoreComponent, feature)
{
    m_mode = "interpolate"; //TODO: set this in config
}

TargetPhraseCollection* PhraseDictionaryInterpolation::CreateTargetPhraseCollection(targetPhraseCollectionStatistics stats) const
{
	std::vector<std::vector<float> > modelWeights;
	modelWeights = getWeights(m_numScoreComponent, true);

    TargetPhraseCollection *ret = new TargetPhraseCollection();

    //interpolation of phrase penalty is skipped, and fixed-value (2.718) is used instead. results will be screwed up if phrase penalty is not last feature
	size_t numInterpWeights = m_numScoreComponent-1;

    for ( std::map< std::string, targetPhraseStatistics*>::const_iterator iter = stats.allStats->begin(); iter != stats.allStats->end(); ++iter ) {

        targetPhraseStatistics * statistics = iter->second;

        Scores scoreVector(m_numScoreComponent);
        vector<float> rawScores(m_numModels);

        for (size_t j = 0; j < numInterpWeights; ++j) {
        	for(size_t i = 0; i < m_numModels; ++i) {
        		rawScores[i] = statistics->p[i][j];
//        		scoreVector[i] = TransformScore(std::inner_product(statistics->p[i].begin(), statistics->p[i].end(), modelWeights[i].begin(), 0.0));
        	}

        	scoreVector[j] = TransformScore(std::inner_product(rawScores.begin(), rawScores.end(), modelWeights[j].begin(), 0.0));
        }

        //assuming that last value is phrase penalty
        scoreVector[numInterpWeights] = 1.0;

        statistics->targetPhrase->SetScore(m_feature, scoreVector, ScoreComponentCollection(), m_weight, m_weightWP, *m_languageModels);

        ret->Add(statistics->targetPhrase);

        delete statistics;
    }
    return ret;
}

} //namespace
