#pragma once

#include <QVariant>


namespace database {
  
/// describes last synx state of local and remote file
struct entry_t {
  
  entry_t(const QString& f, const QString& n, const stat_t& ls, const stat_t& rs, bool d)
    :folder(f), name(n), local(ls), remote(rs), dir(d), bad(false) {}
      
  entry_t(const QString& f, const QString& n, const QVariantMap& data) {
    folder = f;
    name = n;

    dir = data["dir"].value<bool>();
    bad = data["bad"].value<bool>();
    
    local = stat_t(data["local"].value<QVariantMap>());
    remote = stat_t(data["remote"].value<QVariantMap>());
  }
  
  entry_t() : dir(false), bad(false) {}
  
  bool empty() const {return folder.isEmpty() && name.isEmpty() && local.empty() && remote.empty();}
  
  QVariantMap dump() const {
    QVariantMap ret;
    ret["dir"] = dir;
    ret["bad"] = bad;
    
    ret["local"] = local.dump();
    ret["remote"] = remote.dump();
    return ret;
  }
  
  QString filePath() const {
    return QString(folder + "/" + name).replace("//", "");
  }
  
  QString folder;
  QString name;
  
  stat_t local;    
  stat_t remote;
  
  bool dir;
  bool bad;
};
  
}