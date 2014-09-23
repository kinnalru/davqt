#pragma once


namespace database {
  
/// describes last synx state of local and remote file
struct entry_t {
  
  entry_t(const QString& f, const QString& n, const stat_t& ls, const stat_t& rs, bool d)
      :folder(f), name(n), local(ls), remote(rs), dir(d), bad(false) {}
  
  entry_t() : dir(false), bad(false) {}
  
  bool empty() const {return folder.isEmpty() && name.isEmpty() && local.empty() && remote.empty();}
  
  QString folder;
  QString name;
  
  stat_t local;    
  stat_t remote;
  
  bool dir;
  bool bad;
};
  
}