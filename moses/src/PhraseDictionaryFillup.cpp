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
#include "PhraseDictionaryFillup.h"

using namespace std;

namespace Moses

{
  PhraseDictionaryFillup::PhraseDictionaryFillup(size_t numScoreComponent,
      PhraseDictionaryFeature* feature) :
    PhraseDictionaryMultiModel(numScoreComponent, feature)
  {
    m_mode = "fillup"; //TODO: set this in config
    const StaticData &staticData = StaticData::Instance();
    m_fillSourceMaxLength = staticData.GetMaxSourcePhraseFillup();
  }

  TargetPhraseCollection*
  PhraseDictionaryFillup::CreateTargetPhraseCollection(
      targetPhraseCollectionStatistics stats) const
  {
    VERBOSE(2, "Fill-up: CreateTargetPhraseCollection" << std::endl);
    // Get the in-domain ranking of the phrase tables, instead of the mixture weights
    std::vector < size_t > modelRanking;
    modelRanking = getRanking();

    TargetPhraseCollection *ret = new TargetPhraseCollection();

    // Only perform fill-up if the length of the source phrase is below the limit
    std::vector<FactorType> wordFactors;
    wordFactors.push_back(0);

    bool fillThisPhrase = m_fillSourceMaxLength > 0
        ? stats.sourcePhrase->GetSize() <= m_fillSourceMaxLength : true;
    VERBOSE(2, "\"" << stats.sourcePhrase->GetStringRep(wordFactors) << "\" <= " << m_fillSourceMaxLength << " tokens ? " << fillThisPhrase << std::endl);

    // Additional scores correspond to fill-up features: one for each additional "out-of-domain" PT
    //Scores scoreVector(m_numScoreComponent + m_numModels - 1);
    //vector<float> rawScores(m_numScoreComponent);

    Scores scoreVector(m_numScoreComponent);
    size_t numRawScores = m_numScoreComponent - m_numModels + 1;
    vector<float> rawScores(m_numScoreComponent - m_numModels + 1);

    VERBOSE(2, "scoreVector.size(): " << m_numScoreComponent << std::endl);
    VERBOSE(2, "rawScores.size(): " << numRawScores << std::endl);

    // Keep track if the phrase was found so far in the PT search
    bool phraseFound = false;

    // Iterate through each targetPhrase
    for (std::map<std::string, targetPhraseStatistics*>::const_iterator iter =
        stats.allStats->begin(); iter != stats.allStats->end(); ++iter)
    {
      targetPhraseStatistics* statistics = iter->second;

      rawScores.clear();
//      scoreVector.clear();
      phraseFound = false;

      // Look up targetPhrase's scores in PTs according to their rank (e.g. how close they are to being in-domain data);
      // only use the scores from the highest ranked PT that contains this targetPhrase.
      for (size_t rankIndex = 0; rankIndex < modelRanking.size(); ++rankIndex)
      {

        if (!fillThisPhrase && rankIndex > 0)
          // Don't fill-up if looking at lower ranks.
          break;

        // Translate current rank into the actual PT index
        size_t modelIndex = modelRanking[rankIndex];

        // Read targetPhrase scores from the current phrase table
        for (size_t scoreIndex = 0; scoreIndex < numRawScores; ++scoreIndex)
        {

          // If any of the scores are nonzero, then the targetPhrase appears in the current PT.
          if (statistics->p[modelIndex][scoreIndex] > 0)
          {
            phraseFound = true;
          }

          rawScores[scoreIndex] = statistics->p[modelIndex][scoreIndex];
        }

        // Stop looking up scores for this targetPhrase if it was found in the current PT.
        if (phraseFound)
        {
          // Copy the scores from the current PT to scoreVector
          //for (size_t scoreIndex = 0; scoreIndex < m_numScoreComponent; ++scoreIndex) {
          for (size_t scoreIndex = 0; scoreIndex < numRawScores; ++scoreIndex)
          {
            scoreVector[scoreIndex] = TransformScore(rawScores[scoreIndex]);
          }

          // Activate the feature corresponding to the model rank
//          VERBOSE(2, "targetPhrase drawn from PT rank " << rankIndex << std::endl);
//          VERBOSE(2, "Table " << rankIndex << " <- \"" << statistics->targetPhrase->GetStringRep(wordFactors)
//              << "\"" << std::endl);
          if (rankIndex > 0)
          {
            //scoreVector[m_numScoreComponent + rankIndex - 1] = 1.0;
            scoreVector[numRawScores + rankIndex - 1] = 1.0;
          }

          for (size_t scoreIndex = 0; scoreIndex < m_numScoreComponent; scoreIndex++) {
            VERBOSE(2, scoreVector[scoreIndex] << " ");
          }
          VERBOSE(2, "<- (" << rankIndex << ") " << statistics->targetPhrase->GetStringRep(wordFactors) << std::endl);

          VERBOSE(3, "scoreVector.size(): " << scoreVector.size() << " m_weight.size() " << m_weight.size() << std::endl);
          statistics->targetPhrase->SetScore(m_feature, scoreVector,
              ScoreComponentCollection(), m_weight, m_weightWP, *m_languageModels);

          ret->Add(statistics->targetPhrase);

          // Don't look at lower-ranked PTs
          break;
        }
      }

      delete statistics;
    }
    return ret;
  }

} //namespace
