#pragma once

#ifndef ASR_DEVIATION_SCORER_H_
#define ASR_DEVIATION_SCORER_H_

#include <string>
#include <vector>
#include "Types.h"
#include "StatisticsBasedScorer.h"
#include "CderScorer.h"

namespace MosesTuning
{

class AsrDeviationScorer: public StatisticsBasedScorer {

public:
	explicit AsrDeviationScorer(const std::string& config = "");
	~AsrDeviationScorer();

	void setSourceFile(const std::string& sourceFile);
	virtual void setReferenceFiles(const std::vector<std::string>& referenceFiles);

	virtual void prepareStats(std::size_t sid, const std::string& text, ScoreStats& entry);

	virtual std::size_t NumberOfScores() const {
		return 3;
	}

	virtual float calculateScore(const std::vector<ScoreStatsType>& comps) const;

private:
	CderScorer* m_wer_src_ref;
	CderScorer* m_wer_hyp_src;
	CderScorer* m_wer_hyp_ref;

	std::string m_sourceFile;

	typedef std::vector<int> sent_t;
	std::vector<std::string> m_src_sentences;
	std::vector<std::vector<sent_t> > m_ref_sentences;
};

}

#endif /* ASR_DEVIATION_SCORER_H_ */
