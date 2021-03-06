//===--- DiagnosticIDs.cpp - Diagnostic IDs Handling ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Diagnostic IDs-related interfaces.
//
//===----------------------------------------------------------------------===//

#include "fort/Basic/DiagnosticIDs.h"
#include "fort/Basic/AllDiagnostics.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SourceMgr.h"
#include <map>
using namespace fort;

// FIXME: add other diagnostics

//===----------------------------------------------------------------------===//
// Builtin Diagnostic information
//===----------------------------------------------------------------------===//

namespace {

// Diagnostic classes.
enum {
  CLASS_NOTE = 0x01,
  CLASS_WARNING = 0x02,
  CLASS_EXTENSION = 0x03,
  CLASS_ERROR = 0x04
};

struct StaticDiagInfoRec {
  unsigned short DiagID;
  unsigned Mapping : 3;
  unsigned Class : 3;
  unsigned SFINAE : 1;
  unsigned AccessControl : 1;
  unsigned WarnNoWerror : 1;
  unsigned WarnShowInSystemHeader : 1;
  unsigned Category : 5;

  uint16_t OptionGroupIndex;

  uint16_t DescriptionLen;
  const char *DescriptionStr;

  unsigned getOptionGroupIndex() const { return OptionGroupIndex; }

  StringRef getDescription() const {
    return StringRef(DescriptionStr, DescriptionLen);
  }

  bool operator<(const StaticDiagInfoRec &RHS) const {
    return DiagID < RHS.DiagID;
  }
};

} // namespace

static const StaticDiagInfoRec StaticDiagInfo[] = {
#define DIAG(ENUM, CLASS, DEFAULT_MAPPING, DESC, GROUP, SFINAE, ACCESS,        \
             NOWERROR, SHOWINSYSHEADER, CATEGORY)                              \
  {diag::ENUM, DEFAULT_MAPPING, CLASS,    SFINAE, ACCESS,                      \
   NOWERROR,   SHOWINSYSHEADER, CATEGORY, GROUP,  STR_SIZE(DESC, uint16_t),    \
   DESC},
#include "fort/Basic/DiagnosticCommonKinds.inc"
#include "fort/Basic/DiagnosticDriverKinds.inc"
#include "fort/Basic/DiagnosticFrontendKinds.inc"
#include "fort/Basic/DiagnosticLexKinds.inc"
#include "fort/Basic/DiagnosticParseKinds.inc"
#include "fort/Basic/DiagnosticSemaKinds.inc"
#undef DIAG
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

static const unsigned StaticDiagInfoSize =
    sizeof(StaticDiagInfo) / sizeof(StaticDiagInfo[0]) - 1;

/// GetDiagInfo - Return the StaticDiagInfoRec entry for the specified DiagID,
/// or null if the ID is invalid.
static const StaticDiagInfoRec *GetDiagInfo(unsigned DiagID) {
  // If assertions are enabled, verify that the StaticDiagInfo array is sorted.
#ifndef NDEBUG
  static bool IsFirst = true;
  if (IsFirst) {
    for (unsigned i = 1; i != StaticDiagInfoSize; ++i) {
      assert(StaticDiagInfo[i - 1].DiagID != StaticDiagInfo[i].DiagID &&
             "Diag ID conflict, the enums at the start of fort::diag (in "
             "DiagnosticIDs.h) probably need to be increased");

      assert(StaticDiagInfo[i - 1] < StaticDiagInfo[i] &&
             "Improperly sorted diag info");
    }
    IsFirst = false;
  }
#endif

  // Out of bounds diag. Can't be in the table.
  using namespace diag;
  if (DiagID >= DIAG_UPPER_LIMIT)
    return 0;

  // Compute the index of the requested diagnostic in the static table.
  // 1. Add the number of diagnostics in each category preceeding the
  //    diagnostic and of the category the diagnostic is in. This gives us
  //    the offset of the category in the table.
  // 2. Subtract the number of IDs in each category from our ID. This gives us
  //    the offset of the diagnostic in the category.
  // This is cheaper than a binary search on the table as it doesn't touch
  // memory at all.
  unsigned Offset = 0;
  unsigned ID = DiagID;

#define DIAG_START_COMMON 0 // Sentinel value.
#define CATEGORY(NAME, PREV)                                                   \
  if (DiagID > DIAG_START_##NAME) {                                            \
    Offset += NUM_BUILTIN_##PREV##_DIAGNOSTICS - DIAG_START_##PREV - 1;        \
    ID -= DIAG_START_##NAME - DIAG_START_##PREV;                               \
  }
  CATEGORY(DRIVER, COMMON)
  CATEGORY(FRONTEND, DRIVER)
  // CATEGORY(SERIALIZATION, FRONTEND)
  CATEGORY(LEX, FRONTEND)
  CATEGORY(PARSE, LEX)
  // CATEGORY(AST, PARSE)
  // CATEGORY(COMMENT, AST)
  CATEGORY(SEMA, PARSE)
// CATEGORY(ANALYSIS, SEMA)
#undef CATEGORY
#undef DIAG_START_COMMON

  // Avoid out of bounds reads.
  if (ID + Offset >= StaticDiagInfoSize)
    return 0;

  assert(ID < StaticDiagInfoSize && Offset < StaticDiagInfoSize);

  const StaticDiagInfoRec *Found = &StaticDiagInfo[ID + Offset];
  // If the diag id doesn't match we found a different diag, abort. This can
  // happen when this function is called with an ID that points into a hole in
  // the diagID space.
  if (Found->DiagID != DiagID)
    return 0;
  return Found;
}

static DiagnosticMappingInfo GetDefaultDiagMappingInfo(unsigned DiagID) {
  DiagnosticMappingInfo Info = DiagnosticMappingInfo::Make(
      diag::MAP_FATAL, /*IsUser=*/false, /*IsPragma=*/false);

  if (const StaticDiagInfoRec *StaticInfo = GetDiagInfo(DiagID)) {
    Info.setMapping((diag::Mapping)StaticInfo->Mapping);

    if (StaticInfo->WarnNoWerror) {
      assert(Info.getMapping() == diag::MAP_WARNING &&
             "Unexpected mapping with no-Werror bit!");
      Info.setNoWarningAsError(true);
    }

    if (StaticInfo->WarnShowInSystemHeader) {
      assert(Info.getMapping() == diag::MAP_WARNING &&
             "Unexpected mapping with show-in-system-header bit!");
      Info.setShowInSystemHeader(true);
    }
  }

  return Info;
}

/// getCategoryNumberForDiag - Return the category number that a specified
/// DiagID belongs to, or 0 if no category.
unsigned DiagnosticIDs::getCategoryNumberForDiag(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Category;
  return 0;
}

namespace {
// The diagnostic category names.
struct StaticDiagCategoryRec {
  const char *NameStr;
  uint8_t NameLen;

  StringRef getName() const { return StringRef(NameStr, NameLen); }
};
} // namespace

// Unfortunately, the split between DiagnosticIDs and Diagnostic is not
// particularly clean, but for now we just implement this method here so we can
// access GetDefaultDiagMapping.
DiagnosticMappingInfo &
DiagnosticsEngine::DiagState::getOrAddMappingInfo(diag::kind Diag) {
  std::pair<iterator, bool> Result =
      DiagMap.insert(std::make_pair(Diag, DiagnosticMappingInfo()));

  // Initialize the entry if we added it.
  if (Result.second)
    Result.first->second = GetDefaultDiagMappingInfo(Diag);

  return Result.first->second;
}

DiagnosticIDs::SFINAEResponse
DiagnosticIDs::getDiagnosticSFINAEResponse(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID)) {
    if (Info->AccessControl)
      return SFINAE_AccessControl;

    if (!Info->SFINAE)
      return SFINAE_Report;

    if (Info->Class == CLASS_ERROR)
      return SFINAE_SubstitutionFailure;

    // Suppress notes, warnings, and extensions;
    return SFINAE_Suppress;
  }

  return SFINAE_Report;
}

/// getBuiltinDiagClass - Return the class field of the diagnostic.
///
static unsigned getBuiltinDiagClass(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Class;
  return ~0U;
}

//===----------------------------------------------------------------------===//
// Custom Diagnostic information
//===----------------------------------------------------------------------===//

namespace fort {
namespace diag {
class CustomDiagInfo {
  typedef std::pair<DiagnosticIDs::Level, std::string> DiagDesc;
  std::vector<DiagDesc> DiagInfo;
  std::map<DiagDesc, unsigned> DiagIDs;

public:
  /// getDescription - Return the description of the specified custom
  /// diagnostic.
  StringRef getDescription(unsigned DiagID) const {
    assert(DiagID - DIAG_UPPER_LIMIT < DiagInfo.size() &&
           "Invalid diagnostic ID");
    return DiagInfo[DiagID - DIAG_UPPER_LIMIT].second;
  }

  /// getLevel - Return the level of the specified custom diagnostic.
  DiagnosticIDs::Level getLevel(unsigned DiagID) const {
    assert(DiagID - DIAG_UPPER_LIMIT < DiagInfo.size() &&
           "Invalid diagnostic ID");
    return DiagInfo[DiagID - DIAG_UPPER_LIMIT].first;
  }

  unsigned getOrCreateDiagID(DiagnosticIDs::Level L, StringRef Message,
                             DiagnosticIDs &Diags) {
    DiagDesc D(L, Message);
    // Check to see if it already exists.
    std::map<DiagDesc, unsigned>::iterator I = DiagIDs.lower_bound(D);
    if (I != DiagIDs.end() && I->first == D)
      return I->second;

    // If not, assign a new ID.
    unsigned ID = DiagInfo.size() + DIAG_UPPER_LIMIT;
    DiagIDs.insert(std::make_pair(D, ID));
    DiagInfo.push_back(D);
    return ID;
  }
};

} // namespace diag
} // namespace fort

/// getDiagnosticLevel - Based on the way the client configured the
/// DiagnosticsEngine object, classify the specified diagnostic ID into a Level,
/// by consumable the DiagnosticClient.
DiagnosticIDs::Level
DiagnosticIDs::getDiagnosticLevel(unsigned DiagID, SourceLocation Loc,
                                  const DiagnosticsEngine &Diag) const {
  // Handle custom diagnostics, which cannot be mapped.
  if (DiagID >= diag::DIAG_UPPER_LIMIT)
    return CustomDiagInfo->getLevel(DiagID);

  unsigned DiagClass = getBuiltinDiagClass(DiagID);
  if (DiagClass == CLASS_NOTE)
    return DiagnosticIDs::Note;
  return getDiagnosticLevel(DiagID, DiagClass, Loc, Diag);
}

/// \brief Based on the way the client configured the Diagnostic
/// object, classify the specified diagnostic ID into a Level, consumable by
/// the DiagnosticClient.
///
/// \param Loc The source location we are interested in finding out the
/// diagnostic state. Can be null in order to query the latest state.
DiagnosticIDs::Level
DiagnosticIDs::getDiagnosticLevel(unsigned DiagID, unsigned DiagClass,
                                  SourceLocation Loc,
                                  const DiagnosticsEngine &Diag) const {
  // Specific non-error diagnostics may be mapped to various levels from ignored
  // to error.  Errors can only be mapped to fatal.
  DiagnosticIDs::Level Result = DiagnosticIDs::Fatal;

  DiagnosticsEngine::DiagStatePointsTy::iterator Pos =
      Diag.GetDiagStatePointForLoc(Loc);
  DiagnosticsEngine::DiagState *State = Pos->State;

  // Get the mapping information, or compute it lazily.
  DiagnosticMappingInfo &MappingInfo =
      State->getOrAddMappingInfo((diag::kind)DiagID);

  switch (MappingInfo.getMapping()) {
  case diag::MAP_IGNORE:
    Result = DiagnosticIDs::Ignored;
    break;
  case diag::MAP_WARNING:
    Result = DiagnosticIDs::Warning;
    break;
  case diag::MAP_ERROR:
    Result = DiagnosticIDs::Error;
    break;
  case diag::MAP_FATAL:
    Result = DiagnosticIDs::Fatal;
    break;
  }

  return Result;
}

struct fort::WarningOption {
  // Be safe with the size of 'NameLen' because we don't statically check if
  // the size will fit in the field; the struct size won't decrease with a
  // shorter type anyway.
  size_t NameLen;
  const char *NameStr;
  const short *Members;
  const short *SubGroups;

  StringRef getName() const { return StringRef(NameStr, NameLen); }
};

#define GET_DIAG_ARRAYS
#include "fort/Basic/DiagnosticGroups.inc"
#undef GET_DIAG_ARRAYS

// Second the table of options, sorted by name for fast binary lookup.
static const WarningOption OptionTable[] = {
#define GET_DIAG_TABLE
#include "fort/Basic/DiagnosticGroups.inc"
#undef GET_DIAG_TABLE
};
static const size_t OptionTableSize =
    sizeof(OptionTable) / sizeof(OptionTable[0]);

static bool WarningOptionCompare(const WarningOption &LHS,
                                 const WarningOption &RHS) {
  return LHS.getName() < RHS.getName();
}

//===----------------------------------------------------------------------===//
// Common Diagnostic implementation
//===----------------------------------------------------------------------===//

DiagnosticIDs::DiagnosticIDs() { CustomDiagInfo = 0; }

DiagnosticIDs::~DiagnosticIDs() { delete CustomDiagInfo; }

/// getCustomDiagID - Return an ID for a diagnostic with the specified message
/// and level.  If this is the first request for this diagnostic, it is
/// registered and created, otherwise the existing ID is returned.
unsigned DiagnosticIDs::getCustomDiagID(Level L, StringRef Message) {
  if (CustomDiagInfo == 0)
    CustomDiagInfo = new diag::CustomDiagInfo();
  return CustomDiagInfo->getOrCreateDiagID(L, Message, *this);
}

/// isBuiltinWarningOrExtension - Return true if the unmapped diagnostic
/// level of the specified diagnostic ID is a Warning or Extension.
/// This only works on builtin diagnostics, not custom ones, and is not legal to
/// call on NOTEs.
bool DiagnosticIDs::isBuiltinWarningOrExtension(unsigned DiagID) {
  return DiagID < diag::DIAG_UPPER_LIMIT &&
         getBuiltinDiagClass(DiagID) != CLASS_ERROR;
}

/// \brief Determine whether the given built-in diagnostic ID is a
/// Note.
bool DiagnosticIDs::isBuiltinNote(unsigned DiagID) {
  return DiagID < diag::DIAG_UPPER_LIMIT &&
         getBuiltinDiagClass(DiagID) == CLASS_NOTE;
}

/// isBuiltinExtensionDiag - Determine whether the given built-in diagnostic
/// ID is for an extension of some sort.  This also returns EnabledByDefault,
/// which is set to indicate whether the diagnostic is ignored by default (in
/// which case -pedantic enables it) or treated as a warning/error by default.
///
bool DiagnosticIDs::isBuiltinExtensionDiag(unsigned DiagID,
                                           bool &EnabledByDefault) {
  if (DiagID >= diag::DIAG_UPPER_LIMIT ||
      getBuiltinDiagClass(DiagID) != CLASS_EXTENSION)
    return false;

  EnabledByDefault =
      GetDefaultDiagMappingInfo(DiagID).getMapping() != diag::MAP_IGNORE;
  return true;
}

bool DiagnosticIDs::isDefaultMappingAsError(unsigned DiagID) {
  if (DiagID >= diag::DIAG_UPPER_LIMIT)
    return false;

  return GetDefaultDiagMappingInfo(DiagID).getMapping() == diag::MAP_ERROR;
}

/// getDescription - Given a diagnostic ID, return a description of the
/// issue.
StringRef DiagnosticIDs::getDescription(unsigned DiagID) const {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->getDescription();
  return CustomDiagInfo->getDescription(DiagID);
}

/// getWarningOptionForDiag - Return the lowest-level warning option that
/// enables the specified diagnostic.  If there is no -Wfoo flag that controls
/// the diagnostic, this returns null.
StringRef DiagnosticIDs::getWarningOptionForDiag(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return OptionTable[Info->getOptionGroupIndex()].getName();
  return StringRef();
}

void DiagnosticIDs::getDiagnosticsInGroup(
    const WarningOption *Group, SmallVectorImpl<diag::kind> &Diags) const {
  // Add the members of the option diagnostic set.
  if (const short *Member = Group->Members) {
    for (; *Member != -1; ++Member)
      Diags.push_back(*Member);
  }

  // Add the members of the subgroups.
  if (const short *SubGroups = Group->SubGroups) {
    for (; *SubGroups != (short)-1; ++SubGroups)
      getDiagnosticsInGroup(&OptionTable[(short)*SubGroups], Diags);
  }
}

bool DiagnosticIDs::getDiagnosticsInGroup(
    StringRef Group, SmallVectorImpl<diag::kind> &Diags) const {
  WarningOption Key = {Group.size(), Group.data(), 0, 0};
  const WarningOption *Found = std::lower_bound(
      OptionTable, OptionTable + OptionTableSize, Key, WarningOptionCompare);
  if (Found == OptionTable + OptionTableSize || Found->getName() != Group)
    return true; // Option not found.

  getDiagnosticsInGroup(Found, Diags);
  return false;
}

void DiagnosticIDs::getAllDiagnostics(
    SmallVectorImpl<diag::kind> &Diags) const {
  for (unsigned i = 0; i != StaticDiagInfoSize; ++i)
    Diags.push_back(StaticDiagInfo[i].DiagID);
}

StringRef DiagnosticIDs::getNearestWarningOption(StringRef Group) {
  StringRef Best;
  unsigned BestDistance = Group.size() + 1; // Sanity threshold.
  for (const WarningOption *i = OptionTable, *e = OptionTable + OptionTableSize;
       i != e; ++i) {
    // Don't suggest ignored warning flags.
    if (!i->Members && !i->SubGroups)
      continue;

    unsigned Distance = i->getName().edit_distance(Group, true, BestDistance);
    if (Distance == BestDistance) {
      // Two matches with the same distance, don't prefer one over the other.
      Best = "";
    } else if (Distance < BestDistance) {
      // This is a better match.
      Best = i->getName();
      BestDistance = Distance;
    }
  }

  return Best;
}

/// ProcessDiag - This is the method used to report a diagnostic that is
/// finally fully formed.
bool DiagnosticIDs::ProcessDiag(DiagnosticsEngine &Diag) const {
  Diagnostic Info(&Diag);

  // if (Diag.SuppressAllDiagnostics)
  //  return false;

  assert(Diag.getClient() && "DiagnosticClient not set!");

  // Figure out the diagnostic level of this message.
  unsigned DiagID = Info.getID();
  DiagnosticIDs::Level DiagLevel =
      getDiagnosticLevel(DiagID, Info.getLocation(), Diag);

  if (DiagLevel != DiagnosticIDs::Note) {
    // Record that a fatal error occurred only when we see a second
    // non-note diagnostic. This allows notes to be attached to the
    // fatal error, but suppresses any diagnostics that follow those
    // notes.
    // if (Diag.LastDiagLevel == DiagnosticIDs::Fatal)
    //  Diag.FatalErrorOccurred = true;

    // Diag.LastDiagLevel = DiagLevel;
  }

  // Update counts for DiagnosticErrorTrap even if a fatal error occurred.
  if (DiagLevel >= DiagnosticIDs::Error) {
    ++Diag.TrapNumErrorsOccurred;
    // if (isUnrecoverable(DiagID))
    //  ++Diag.TrapNumUnrecoverableErrorsOccurred;
  }

  // If a fatal error has already been emitted, silence all subsequent
  // diagnostics.
  if (Diag.FatalErrorOccurred) {
    if (DiagLevel >= DiagnosticIDs::Error &&
        Diag.Client->IncludeInDiagnosticCounts()) {
      ++Diag.NumErrors;
      ++Diag.NumErrorsSuppressed;
    }

    return false;
  }

  Diag.LastDiagLevel = DiagLevel;

  // If the client doesn't care about this message, don't issue it.  If this is
  // a note and the last real diagnostic was ignored, ignore it too.
  if (DiagLevel == DiagnosticIDs::Ignored ||
      (DiagLevel == DiagnosticIDs::Note &&
       Diag.LastDiagLevel == DiagnosticIDs::Ignored))
    return false;

  if (DiagLevel >= DiagnosticIDs::Error) {
    // if (isUnrecoverable(DiagID))
    //  Diag.UnrecoverableErrorOccurred = true;

    // Warnings which have been upgraded to errors do not prevent compilation.
    if (isDefaultMappingAsError(DiagID))
      Diag.UncompilableErrorOccurred = true;

    Diag.ErrorOccurred = true;
    if (Diag.Client->IncludeInDiagnosticCounts()) {
      ++Diag.NumErrors;
    }

    // If we've emitted a lot of errors, emit a fatal error instead of it to
    // stop a flood of bogus errors.
    /*if (Diag.ErrorLimit && Diag.NumErrors > Diag.ErrorLimit &&
        DiagLevel == DiagnosticIDs::Error) {
      Diag.SetDelayedDiagnostic(diag::fatal_too_many_errors);
      return false;
    }*/
  }

  // Finally, report it.
  EmitDiag(Diag, DiagLevel);
  return true;
}

void DiagnosticIDs::EmitDiag(DiagnosticsEngine &Diag, Level DiagLevel) const {
  Diagnostic Info(&Diag);
  assert(DiagLevel != DiagnosticIDs::Ignored &&
         "Cannot emit ignored diagnostics!");

  // Render the diagnostic message into a temporary buffer eagerly. We'll use
  // this later as we print out the diagnostic to the terminal.
  llvm::SmallString<100> OutStr;
  Info.FormatDiagnostic(OutStr);
  DiagnosticsEngine::Level Lvl;
  switch (DiagLevel) {
  case Ignored:
    Lvl = DiagnosticsEngine::Ignored;
    break;
  case Note:
    Lvl = DiagnosticsEngine::Note;
    break;
  case Warning:
    Lvl = DiagnosticsEngine::Warning;
    break;
  case Error:
    Lvl = DiagnosticsEngine::Error;
    break;
  case Fatal:
    Lvl = DiagnosticsEngine::Fatal;
    break;
  }
  Diag.Client->HandleDiagnostic(
      Lvl, Info.getLocation(), llvm::Twine(OutStr));

  Diag.CurDiagID = ~0U;
}
