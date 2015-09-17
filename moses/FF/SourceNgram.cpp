/*
 * SourceNgram.cpp
 *
 *  Created on: Sep 15, 2015
 *      Author: prmathur
 */

#include "SourceNgram.h"
#include "util/string_piece_hash.hh"
#include "util/exception.hh"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "moses/StaticData.h"
#include "moses/InputPath.h"
#include "util/file_piece.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"
#include "util/double-conversion/double-conversion.h"

namespace Moses {

SourceNgram::SourceNgram(const std::string &line)
	:StatelessFeatureFunction(0, line){
	// TODO Auto-generated constructor stub
	m_ignorePunctuation = true;
	m_size=200;
	m_src_store.clear();
	m_srcfeatureFile="";
	ReadParameters();
}
void SourceNgram::SetParameter(const std::string& key, const std::string& value)
{
  std::cerr<<"Key :: "<<key<<"\t Value :: "<<value<<std::endl;
  if (key == "ignore-punctuation") {
    m_ignorePunctuation = Scan<bool>(value);
  } else if (key == "path") {
    m_weightFile = value;
  } else if(key == "src-feature-file"){
	  m_srcfeatureFile = value;
  } else if(key == "normalize"){
	  m_normalize = Scan<bool>(value);
  } else {
    StatelessFeatureFunction::SetParameter(key, value);
  }
}

void SourceNgram::Load()
{
	std::string extension="";
	std::string::size_type idx = m_srcfeatureFile.rfind('.');
	if(idx != std::string::npos)
		extension = m_srcfeatureFile.substr(idx+1);
// source features
	if(extension.compare("gz")==0){
		double_conversion::StringToDoubleConverter converter(double_conversion::StringToDoubleConverter::NO_FLAGS, NAN, NAN, "inf", "nan");
		std::ostream *progress = NULL;
		IFVERBOSE(1) progress = &std::cerr;
		// reused variables
		util::FilePiece in(m_srcfeatureFile.c_str(), progress);
		std::vector<float> scoreVector;
		StringPiece line;
		while(true) {
			try {
				line = in.ReadLine();
			} catch (const util::EndOfFileException &e) {
				break;
			}
			util::TokenIter<util::MultiCharacter> pipes(line, "\t");
			StringPiece PhraseString(*pipes);
			StringPiece scoreString(*++pipes);

			std::string Phr = PhraseString.as_string();
			bool punc_flag=false;
			if(punc_flag==true) continue;
			std::string s = scoreString.as_string();
			double score=0;
			if(s.empty()){
				m_src_store[Phr] = 0;
				continue;
			}
			boost::algorithm::trim(s);
			std::stringstream ss, featureName;
			ss << s;
			ss >> score;
			m_src_store[Phr] = score;
		}
	}
}

void SourceNgram::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{ // implements max over time pooling operation to get a global feature vector for a phrase
	const Phrase& source = inputPath.GetPhrase();
	double score=0;
	std::ostringstream srcPhr;
	std::string srcStr="";
	for (size_t i = 1; i < source.GetSize(); ++i)
		srcStr.append(source.GetWord(i).GetFactor(0)->GetString().as_string());
	if(m_src_store.find(srcStr)!=m_src_store.end()){
		score = m_src_store.at(srcStr);
	}
	srcPhr << "src_" << srcStr;
	if(score != 0){
		if(m_normalize)
			scoreBreakdown.SparsePlusEquals(srcPhr.str(), (1.0 / ( 1.0 + exp(-score) )));
		else
			scoreBreakdown.SparsePlusEquals(srcPhr.str(), score);
	}
	else {
		scoreBreakdown.SparsePlusEquals(srcPhr.str(), 1);
	}

}

bool SourceNgram::IsUseable(const FactorMask &mask) const
{
  return true;
}

} /* namespace Moses */
