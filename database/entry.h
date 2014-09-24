#pragma once

#include <QVariant>


namespace database {
  
class database_t;
  
/// describes last synx state of local and remote file
struct entry_t {
  
  entry_t() : dir(false) {}
  bool empty() const {return key.isEmpty() && local.empty() && remote.empty();}
  
  QVariantMap dump() const {
    QVariantMap ret;
    ret["dir"] = dir;
    
    ret["local"] = local.dump();
    ret["remote"] = remote.dump();
    return ret;
  }
  
  QString key;
  
  stat_t local;    
  stat_t remote;
  
  bool dir;
  
private:
  friend class database::database_t;
  
  entry_t(const QString& key, const stat_t& ls, const stat_t& rs, bool d)
    : key(key), local(ls), remote(rs), dir(d) {}
      
  entry_t(const QString& key, const QVariantMap& data)
    : key(key)
  {
    dir = data["dir"].value<bool>();
    
    local = stat_t(data["local"].value<QVariantMap>());
    remote = stat_t(data["remote"].value<QVariantMap>());
  }

};
  
}