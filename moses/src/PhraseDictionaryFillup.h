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

#ifndef moses_PhraseDictionaryFillup_h
#define moses_PhraseDictionaryFillup_h

#include "PhraseDictionary.h"
#include "PhraseDictionaryMemory.h"
#include "PhraseDictionaryMultiModel.h"
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

/** Implementation of linearly-interpolated PTs.
 */
class PhraseDictionaryFillup: public PhraseDictionaryMultiModel
{

public:
  PhraseDictionaryFillup(size_t m_numScoreComponent, PhraseDictionaryFeature* feature);
//  virtual ~PhraseDictionaryFillup();
  virtual TargetPhraseCollection* CreateTargetPhraseCollection(targetPhraseCollectionStatistics stats) const;
  inline size_t GetSourceMaxLength() const {
	return m_fillSourceMaxLength;
  }

protected:
  size_t m_fillSourceMaxLength;

};

} // end namespace

#endif
