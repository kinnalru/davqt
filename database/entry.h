#pragma once

#include <QVariant>
#include "types.h"


namespace database {
  
class database_t;
  
/// describes last synx state of local and remote file
struct entry_t {
  
  entry_t() : dir(false) {}
  
  QVariantMap dump() const {
    QVariantMap ret;
    ret["dir"] = dir;
    
    ret["local"] = local.dump();
    ret["remote"] = remote.dump();
    return ret;
  }
  
  QString key;
  
  UrlInfo local;    
  UrlInfo remote;
  
  bool dir;
  
private:
  friend class database::database_t;
  
  entry_t(const QString& key, const UrlInfo& ls, const UrlInfo& rs, bool d)
    : key(key), local(ls), remote(rs), dir(d) {}
      
  entry_t(const QString& key, const QVariantMap& data)
    : key(key)
  {
    dir = data["dir"].value<bool>();
    
    local = UrlInfo(data["local"].value<QVariantMap>());
    remote = UrlInfo(data["remote"].value<QVariantMap>());
  }

};
  
}