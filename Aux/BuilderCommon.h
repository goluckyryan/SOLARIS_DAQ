#ifndef BUILDER_COMMON_H
#define BUILDER_COMMON_H

/// Shared by the offline event builders EventBuilder (.sol) and EventBuilderRaw (.raw): the clock
/// used for the build-time printout, and the bookkeeping of which file of which board is read next.

#include <sys/time.h>
#include <climits>
#include <vector>

inline unsigned int getTime_us(){
  struct timeval t1;
  struct timezone tz;
  gettimeofday(&t1, &tz);
  return (t1.tv_sec) * 1000 * 1000 + t1.tv_usec;
}

/// The files of a run, grouped by board: group[g] lists board g's files in order (_000, _001, ...).
/// Each board reads one file at a time; when it ends, the board's next file takes over.
/// The "active" lists have one entry per board still reading: the file it is on (activeFileID),
/// which board that is (activeGroupID), and the file's place within the board's list (groupIndex).
struct FileGroups {
  std::vector<std::vector<int>> group;
  std::vector<int> activeFileID, activeGroupID, groupIndex;

  /// Every board starts on its first file.
  void Start(){
    activeFileID.clear(); activeGroupID.clear(); groupIndex.clear();
    for( int i = 0; i < (int) group.size(); i++ ){
      groupIndex.push_back(0);
      activeFileID.push_back(group[i][0]);
      activeGroupID.push_back(i);
    }
  }

  bool Done() const { return activeFileID.empty(); }

  /// The board whose current hit is earliest: its file (fileID) and its place in the active list
  /// (groupID). timeOf(id) gives the current hit's timestamp of file id. On equal timestamps the
  /// LAST such board wins -- the behaviour the builders were tuned against. With no active board
  /// fileID and groupID are left unchanged.
  template <class TimeOf>
  void Earliest(TimeOf timeOf, int & fileID, int & groupID) const {
    unsigned long long firstTime = ULLONG_MAX;
    for( int i = 0; i < (int) activeFileID.size(); i++ ){
      const int id = activeFileID[i];
      if( (unsigned long long) timeOf(id) <= firstTime ){
        firstTime = timeOf(id);
        fileID = id;
        groupID = i;
      }
    }
  }

  /// The current file of active board groupID has ended: move that board to its next file (fileID
  /// becomes it), or drop the board if that was its last file.
  void NextFile(int groupID, int & fileID){
    const int origGroup = activeGroupID[groupID];
    groupIndex[groupID] ++;
    if( groupIndex[groupID] < (int) group[origGroup].size() ){
      activeFileID[groupID] = group[origGroup][groupIndex[groupID]];
      fileID = activeFileID[groupID];
    }else{
      activeFileID.erase(activeFileID.begin() + groupID);
      activeGroupID.erase(activeGroupID.begin() + groupID);
      groupIndex.erase(groupIndex.begin() + groupID);
    }
  }

  /// Bytes read so far over the files opened up to now; posOf(id) gives file id's read position.
  template <class PosOf>
  unsigned long BytesRead(PosOf posOf) const {
    unsigned long sum = 0;
    for( int p = 0; p < (int) activeGroupID.size(); p++ ){
      const int gID = activeGroupID[p];
      for( int q = 0; q <= groupIndex[p]; q++ ) sum += posOf(group[gID][q]);
    }
    return sum;
  }

  /// Bytes read over every file of the run (for the final count).
  template <class PosOf>
  unsigned long BytesReadAll(PosOf posOf) const {
    unsigned long sum = 0;
    for( const auto & g : group ) for( int id : g ) sum += posOf(id);
    return sum;
  }
};

#endif
