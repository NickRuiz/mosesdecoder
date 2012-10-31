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

#ifndef moses_PhraseDictionaryMultiModel_h
#define moses_PhraseDictionaryMultiModel_h

#include "PhraseDictionary.h"
#include "PhraseDictionaryMemory.h"
#include "PhraseDictionaryTreeAdaptor.h"
#ifndef WIN32
#include "CompactPT/PhraseDictionaryCompact.h"
#endif


#include <boost/unordered_map.hpp>
#include "StaticData.h"
#include "TargetPhrase.h"
#include "Util.h"
#include "UserMessage.h"

namespace Moses
{

  struct targetPhraseStatistics {
    TargetPhrase *targetPhrase;
    std::vector<std::vector<float> > p;
  };

  struct targetPhraseCollectionStatistics {
	  const Phrase *sourcePhrase;
	  std::map<std::string,targetPhraseStatistics*>* allStats;
  };

/** Implementation of a phrase table with raw counts.
 */
class PhraseDictionaryMultiModel: public PhraseDictionary
{

public:
  PhraseDictionaryMultiModel(size_t m_numScoreComponent, PhraseDictionaryFeature* feature);
  virtual ~PhraseDictionaryMultiModel();
  virtual bool Load(const std::vector<FactorType> &input
            , const std::vector<FactorType> &output
            , const std::vector<std::string> &files
            , const std::vector<float> &weight
            , size_t tableLimit
            , const LMList &languageModels
            , float weightWP);
  virtual TargetPhraseCollection* CreateTargetPhraseCollection(targetPhraseCollectionStatistics stats) const = 0;
//  TargetPhraseCollection* CreateTargetPhraseCollectionLinearInterpolation(std::map<std::string,multiModelStatistics*>* allStats, std::vector<std::vector<float> > &multimodelweights) const;
  virtual std::vector<std::vector<float> > getWeights(size_t numWeights, bool normalize) const;
  virtual std::vector<size_t> getRanking() const;
  virtual std::vector<float> normalizeWeights(std::vector<float> &weights) const;
  virtual void CacheForCleanup(TargetPhraseCollection* tpc);
  virtual void CleanUp(const InputType &source);
  // functions below required by base class
  virtual const TargetPhraseCollection* GetTargetPhraseCollection(const Phrase& src) const;
  virtual void InitializeForInput(InputType const&) {
    /* Don't do anything source specific here as this object is shared between threads.*/
  }
  virtual ChartRuleLookupManager *CreateRuleLookupManager(const InputType&, const ChartCellCollectionBase&);

protected:
  std::string m_mode;
  std::vector<PhraseDictionary*> m_pd;
  std::vector<float> m_weight;
  const LMList *m_languageModels;
  float m_weightWP;
  std::vector<FactorType> m_input;
  std::vector<FactorType> m_output;
  size_t m_numModels;
  PhraseDictionaryFeature* m_feature_load;

  typedef std::vector<TargetPhraseCollection*> PhraseCache;
#ifdef WITH_THREADS
  boost::mutex m_sentenceMutex;
  typedef std::map<boost::thread::id, PhraseCache> SentenceCache;
#else
  typedef PhraseCache SentenceCache;
#endif
  SentenceCache m_sentenceCache;

};

} // end namespace

#endif
