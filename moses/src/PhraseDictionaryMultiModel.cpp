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

#include "PhraseDictionaryMultiModel.h"

using namespace std;

namespace Moses

{
  PhraseDictionaryMultiModel::PhraseDictionaryMultiModel(
      size_t numScoreComponent, PhraseDictionaryFeature* feature) :
    PhraseDictionary(numScoreComponent, feature)
  {
    m_feature_load = feature;
    m_mode = "undefined"; //TODO: set this in config
  }

  PhraseDictionaryMultiModel::~PhraseDictionaryMultiModel()
  {
    RemoveAllInColl( m_pd);
  }

  bool
  PhraseDictionaryMultiModel::Load(const std::vector<FactorType> &input,
      const std::vector<FactorType> &output,
      const std::vector<std::string> &files, const vector<float> &weight,
      size_t tableLimit, const LMList &languageModels, float weightWP)
  {
    m_languageModels = &languageModels;
    m_weight = weight;
    m_weightWP = weightWP;
    m_input = input;
    m_output = output;
    m_tableLimit = tableLimit;

    m_numModels = files.size();
    VERBOSE(2, "Load() -> Number of phrase tables: " << m_numModels << std::endl);

    //how many actual scores there are in the phrase tables
    //so far, equal to number of log-linear scores, but it is allowed to be smaller (for other combination types)
    size_t numPtScores = m_numScoreComponent;

    for (size_t i = 0; i < m_numModels; ++i)
    {
      VERBOSE(2, "Processing PT: " << files[i] << std::endl);
      std::string impl, file, main_table;

      std::string delim = ":";
      size_t delim_pos = files[i].find(delim);
      if (delim_pos >= files[i].size())
      {
        UserMessage::Add(
            "Phrase table must be specified in this format: Implementation:Path");
        CHECK(false);
      }

      impl = files[i].substr(0, delim_pos);
      file = files[i].substr(delim_pos + 1, files[i].size());
      size_t table_limit = 0;
      bool success = false;

      PhraseTableImplementation implementation =
          (PhraseTableImplementation) Scan<int> (impl);

      if (implementation == Memory)
      {

        VERBOSE(2, "Memory PT" << std::endl);
        if (!FileExists(file) && FileExists(file + ".gz"))
          file += ".gz";

        PhraseDictionaryMemory* pdm = new PhraseDictionaryMemory(
            m_numScoreComponent, m_feature_load);
        pdm->SetNumScoreComponentsMultiModel(numPtScores); //instead of complaining about inequal number of scores, silently fill up the score vector with zeroes
        success = pdm->Load(input, output, file, weight, table_limit,
            languageModels, weightWP);
        CHECK(success);
        m_pd.push_back(pdm);
      }
      else if (implementation == Binary)
      {
        VERBOSE(2, "Binary PT" << std::endl);
        const StaticData &staticData = StaticData::Instance();
        PhraseDictionaryTreeAdaptor* pdta =
            new PhraseDictionaryTreeAdaptor(m_numScoreComponent,
                staticData.GetNumInputScores(), m_feature_load);
        pdta->SetNumScoreComponentsMultiModel(m_numScoreComponent); //for compact models, we need to pass number of log-linear components to correctly resize the score vector
        success = pdta->Load(input, output, file, weight, table_limit,
            languageModels, weightWP);
        CHECK(success);
        m_pd.push_back(pdta);
      }
      else if (implementation == Compact)
      {
        VERBOSE(2, "Compact PT" << std::endl);
#ifndef WIN32
        PhraseDictionaryCompact* pdc = new PhraseDictionaryCompact(
            m_numScoreComponent, implementation, m_feature_load);
        pdc->SetNumScoreComponentsMultiModel(m_numScoreComponent); //for compact models, we need to pass number of log-linear components to correctly resize the score vector
        success = pdc->Load(input, output, file, weight, table_limit,
            languageModels, weightWP);
        CHECK(success);
        m_pd.push_back(pdc);
#else
        CHECK(false);
#endif
      }
      else
      {
        UserMessage::Add("phrase table type unknown to multi-model mode");
        CHECK(false);
      }
    }

    return true;
  }

  const TargetPhraseCollection *
  PhraseDictionaryMultiModel::GetTargetPhraseCollection(const Phrase& src) const
  {

    /*
     std::vector<std::vector<float> > multimodelweights;
     multimodelweights = getWeights(m_numScoreComponent, true);

     if (m_mode == "interpolate") {
     //interpolation of phrase penalty is skipped, and fixed-value (2.718) is used instead. results will be screwed up if phrase penalty is not last feature
     size_t numWeights = m_numScoreComponent-1;
     multimodelweights = getWeights(numWeights, true);
     }
     */

    //  std::map<std::string,targetPhraseStatistics*>* allStats = new(std::map<std::string,targetPhraseStatistics*>);
    targetPhraseCollectionStatistics stats;
    stats.sourcePhrase = &src;
    stats.allStats = new (std::map<std::string, targetPhraseStatistics*>);
    size_t phraseCount = 0;

    for (size_t i = 0; i < m_numModels; ++i)
    {

      TargetPhraseCollection *ret_raw =
          (TargetPhraseCollection*) m_pd[i]->GetTargetPhraseCollection(src);
      if (ret_raw != NULL)
      {

        TargetPhraseCollection::iterator iterTargetPhrase;
        for (iterTargetPhrase = ret_raw->begin(); iterTargetPhrase
            != ret_raw->end(); ++iterTargetPhrase)
        {
          TargetPhrase * targetPhrase = *iterTargetPhrase;
          std::vector<float> raw_scores =
              targetPhrase->GetScoreBreakdown().GetScoresForProducer(m_feature);

          std::string targetString = targetPhrase->GetStringRep(m_output);
          if (stats.allStats->find(targetString) == stats.allStats->end())
          {
            phraseCount++;

            targetPhraseStatistics * statistics = new targetPhraseStatistics;
            statistics->targetPhrase = new TargetPhrase(*targetPhrase); //make a copy so that we don't overwrite the original phrase table info

            // Transpose this
            Scores scoreVector(m_numScoreComponent);
            for (size_t j = 0; j < m_numScoreComponent; ++j)
            {
              scoreVector[j] = -raw_scores[j];
            }

            statistics->p.resize(m_numModels);
            for (size_t k = 0; k < m_numModels; ++k)
            {
              statistics->p[k].resize(m_numScoreComponent);
            }

            /*
             Scores scoreVector(m_numScoreComponent);
             statistics->p.resize(m_numScoreComponent);
             for(size_t j = 0; j < m_numScoreComponent; ++j){
             statistics->p[j].resize(m_numModels);
             scoreVector[j] = -raw_scores[j];
             }
             */
//            VERBOSE(2, "Resetting scores on Load() - (" << scoreVector.size()
//                << " scores)" << std::endl);
            statistics->targetPhrase->SetScore(m_feature, scoreVector,
                ScoreComponentCollection(), m_weight, m_weightWP,
                *m_languageModels); // set scores to 0

            (*stats.allStats)[targetString] = statistics;

          }
          targetPhraseStatistics * statistics = (*stats.allStats)[targetString];

          for (size_t j = 0; j < m_numScoreComponent; ++j)
          {
            statistics->p[i][j] = UntransformScore(raw_scores[j]);
          }

          (*stats.allStats)[targetString] = statistics;
        }
      }
    }

    VERBOSE(2, "targetPhrase candidates: " << phraseCount << std::endl);

    TargetPhraseCollection *ret;
    //  if (m_mode == "interpolate") {
    //    ret = CreateTargetPhraseCollection(allStats, multimodelweights);
    //  }
    ret = CreateTargetPhraseCollection(stats);

    ret->NthElement(m_tableLimit); // sort the phrases for pruning later
    const_cast<PhraseDictionaryMultiModel*> (this)->CacheForCleanup(ret);
    delete stats.allStats;

    return ret;
  }

  //TargetPhraseCollection* PhraseDictionaryMultiModel::CreateTargetPhraseCollectionLinearInterpolation(std::map<std::string,multiModelStatistics*>* allStats, std::vector<std::vector<float> > &multimodelweights) const
  //{
  //    TargetPhraseCollection *ret = new TargetPhraseCollection();
  //    for ( std::map< std::string, multiModelStatistics*>::const_iterator iter = allStats->begin(); iter != allStats->end(); ++iter ) {
  //
  //        multiModelStatistics * statistics = iter->second;
  //
  //        Scores scoreVector(m_numScoreComponent);
  //
  //        for(size_t i = 0; i < m_numScoreComponent-1; ++i){
  //            scoreVector[i] = TransformScore(std::inner_product(statistics->p[i].begin(), statistics->p[i].end(), multimodelweights[i].begin(), 0.0));
  //        }
  //
  //        //assuming that last value is phrase penalty
  //        scoreVector[m_numScoreComponent-1] = 1.0;
  //
  //        statistics->targetPhrase->SetScore(m_feature, scoreVector, ScoreComponentCollection(), m_weight, m_weightWP, *m_languageModels);
  //
  //        ret->Add(statistics->targetPhrase);
  //
  //        delete statistics;
  //    }
  //    return ret;
  //}

  std::vector<size_t>
  PhraseDictionaryMultiModel::getRanking() const
  {
    const std::vector<std::pair<size_t, float> >* rank_ptr;
    std::vector < size_t > rankingVector;
    const StaticData &staticData = StaticData::Instance();

    VERBOSE(3, "Start GetMultiModelRankVector()" << std::endl);
    rank_ptr = staticData.GetMultiModelRankVector(); // fall back to rank defined in config
    VERBOSE(3, "End GetMultiModelRankVector()" << std::endl);
    /*
     VERBOSE(1,"Start GetTemporaryMultiModelRankVector()" << std::endl);
     rank_ptr = staticData.GetTemporaryMultiModelRankVector();
     VERBOSE(1,"End GetTemporaryMultiModelRankVector()" << std::endl);

     VERBOSE(1, "Checking rank_ptr" << std::endl);
     // checking rank passed to mosesserver; only valid for this sentence; *don't* raise exception if client rank is malformed
     if (rank_ptr == NULL || rank_ptr->size() == 0) {
     VERBOSE(1, "rank_ptr == NULL or 0 length" << std::endl);
     rank_ptr = staticData.GetMultiModelRankVector(); // fall back to rank defined in config
     }
     else if (rank_ptr->size() != m_numModels) {
     //TODO: can we pass error message to client if weights are malformed?
     std::stringstream strme;
     strme << "Must define rankings for all models (" << m_numModels << "). You have " << rank_ptr->size() << ". Reverting to ranking in config";
     UserMessage::Add(strme.str());
     rank_ptr = staticData.GetMultiModelRankVector(); //fall back to weights defined in config
     }

     */
    VERBOSE(3, "Checking rank_ptr again" << std::endl);
    // checking rank passed to config; only valid for this sentence; raise exception if client rank is malformed
    if (rank_ptr == NULL || rank_ptr->size() == 0)
    {
      for (size_t i = 0; i < m_numModels; i++)
      {
        rankingVector.push_back(i); //uniform weights created online
      }
    }
    else if (rank_ptr->size() != m_numModels)
    {
      //TODO: can we pass error message to client if weights are malformed?
      std::stringstream strme;
      strme << "Must define rankings for all models (" << m_numModels
          << "). You have " << rank_ptr->size() << ".";
      UserMessage::Add(strme.str());
      CHECK(false);
    }
    else
    {
      VERBOSE(3, "Pushing values" << std::endl);
      VERBOSE(2, "Rank order: ");
      std::vector < std::pair<size_t, float> > rankPairs = *rank_ptr;
      for (size_t i = 0; i < rankPairs.size(); ++i)
      {
        VERBOSE(2, rankPairs[i].first << " ");
        rankingVector.push_back(rankPairs[i].first);
      }
      VERBOSE(2, "" << std::endl);
    }

    return rankingVector;
  }

  //TODO: is it worth caching the results as long as weights don't change?
  std::vector<std::vector<float> >
  PhraseDictionaryMultiModel::getWeights(size_t numWeights, bool normalize) const
  {
    const std::vector<float>* weights_ptr;
    std::vector<float> raw_weights;
    const StaticData &staticData = StaticData::Instance();

    weights_ptr = staticData.GetTemporaryMultiModelWeightsVector();

    //checking weights passed to mosesserver; only valid for this sentence; *don't* raise exception if client weights are malformed
    if (weights_ptr == NULL || weights_ptr->size() == 0)
    {
      weights_ptr = staticData.GetMultiModelWeightsVector(); //fall back to weights defined in config
    }
    else if (weights_ptr->size() != m_numModels && weights_ptr->size()
        != m_numModels * numWeights)
    {
      //TODO: can we pass error message to client if weights are malformed?
      std::stringstream strme;
      strme << "Must have either one multimodel weight per model ("
          << m_numModels << "), or one per weighted feature and model ("
          << numWeights << "*" << m_numModels << "). You have "
          << weights_ptr->size() << ". Reverting to weights in config";
      UserMessage::Add(strme.str());
      weights_ptr = staticData.GetMultiModelWeightsVector(); //fall back to weights defined in config
    }

    //checking weights defined in config; only valid for this sentence; raise exception if config weights are malformed
    if (weights_ptr == NULL || weights_ptr->size() == 0)
    {
      for (size_t i = 0; i < m_numModels; i++)
      {
        raw_weights.push_back(1.0 / m_numModels); //uniform weights created online
      }
    }
    else if (weights_ptr->size() != m_numModels && weights_ptr->size()
        != m_numModels * numWeights)
    {
      std::stringstream strme;
      strme << "Must have either one multimodel weight per model ("
          << m_numModels << "), or one per weighted feature and model ("
          << numWeights << "*" << m_numModels << "). You have "
          << raw_weights.size() << ".";
      UserMessage::Add(strme.str());
      CHECK(false);
    }
    else
    {
      raw_weights = *weights_ptr;
    }

    std::vector < std::vector<float> > multimodelweights(numWeights);

    for (size_t i = 0; i < numWeights; i++)
    {
      std::vector<float> weights_onefeature(m_numModels);
      if (raw_weights.size() == m_numModels)
      {
        weights_onefeature = raw_weights;
      }
      else
      {
        copy(raw_weights.begin() + i * m_numModels, raw_weights.begin() + (i
            + 1) * m_numModels, weights_onefeature.begin());
      }
      if (normalize)
      {
        multimodelweights[i] = normalizeWeights(weights_onefeature);
      }
      else
      {
        multimodelweights[i] = weights_onefeature;
      }
    }

    return multimodelweights;
  }

  std::vector<float>
  PhraseDictionaryMultiModel::normalizeWeights(std::vector<float> &weights) const
  {
    std::vector<float> ret(m_numModels);
    float total = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (size_t i = 0; i < weights.size(); i++)
    {
      ret[i] = weights[i] / total;
    }
    return ret;
  }

  ChartRuleLookupManager *
  PhraseDictionaryMultiModel::CreateRuleLookupManager(const InputType&,
      const ChartCellCollectionBase&)
  {
    CHECK(false);
    return 0;
  }

  //copied from PhraseDictionaryCompact; free memory allocated to TargetPhraseCollection (and each TargetPhrase) at end of sentence
  void
  PhraseDictionaryMultiModel::CacheForCleanup(TargetPhraseCollection* tpc)
  {
#ifdef WITH_THREADS
    boost::mutex::scoped_lock lock(m_sentenceMutex);
    PhraseCache &ref = m_sentenceCache[boost::this_thread::get_id()];
#else
    PhraseCache &ref = m_sentenceCache;
#endif
    ref.push_back(tpc);
  }

  void
  PhraseDictionaryMultiModel::CleanUp(const InputType &source)
  {
#ifdef WITH_THREADS
    boost::mutex::scoped_lock lock(m_sentenceMutex);
    PhraseCache &ref = m_sentenceCache[boost::this_thread::get_id()];
#else
    PhraseCache &ref = m_sentenceCache;
#endif
    for (PhraseCache::iterator it = ref.begin(); it != ref.end(); it++)
    {
      delete *it;
    }

    PhraseCache temp;
    temp.swap(ref);

    const StaticData &staticData = StaticData::Instance();
    std::vector<float> empty_vector;
    (const_cast<StaticData&> (staticData)).SetTemporaryMultiModelWeightsVector(
        empty_vector);
  }

} //namespace
