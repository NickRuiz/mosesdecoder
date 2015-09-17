/*
 * SourceNgram.h
 *
 *  Created on: Sep 15, 2015
 *      Author: prmathur
 */

#ifndef MOSES_FF_SOURCENGRAM_H_
#define MOSES_FF_SOURCENGRAM_H_

#include "StatelessFeatureFunction.h"

namespace Moses {

class SourceNgram : public StatelessFeatureFunction {
public:
	SourceNgram(const std::string &line);

	void Load();
	void SetParameter(const std::string& key, const std::string& value);

	bool IsUseable(const FactorMask &mask) const;

	void EvaluateInIsolation(const Phrase &source
			, const TargetPhrase &targetPhrase
			, ScoreComponentCollection &scoreBreakdown
			, ScoreComponentCollection &estimatedFutureScore) const {
	}

	void EvaluateTranslationOptionListWithSourceContext(const InputType &input
			, const TranslationOptionList &translationOptionList) const {
	}
	void EvaluateWithSourceContext(const InputType &input
			, const InputPath &inputPath
			, const TargetPhrase &targetPhrase
			, const StackVec *stackVec
			, ScoreComponentCollection &scoreBreakdown
			, ScoreComponentCollection *estimatedFutureScore = NULL) const;

	void EvaluateWhenApplied(const Hypothesis& hypo,
			ScoreComponentCollection* accumulator) const {
	}

	void EvaluateWhenApplied(const ChartHypothesis& hypo,
			ScoreComponentCollection*) const {
	}

private:
	boost::unordered_map<std::string, float> m_src_store;
	std::string m_weightFile, m_srcfeatureFile;
	int m_size;
	bool m_ignorePunctuation, m_normalize;
};

} /* namespace Moses */

#endif /* MOSES_FF_SOURCENGRAM_H_ */
