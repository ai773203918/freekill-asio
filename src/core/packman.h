// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _PACKMAN_H
#define _PACKMAN_H

class Sqlite3;

class PackMan {

public:
  PackMan();
  ~PackMan();

  std::vector<std::string> &getDisabledPacks();
  /*
  std::vector<std::string> &getPackSummary();
  // server用不到loadSummary，但还是先留着
  void loadSummary(const QString &, bool useThread = false);
  */
  int downloadNewPack(const char *url);
  void enablePack(const char *pack);
  void disablePack(const char *pack);
  int updatePack(const char *pack, const char *hash);
  int upgradePack(const char *pack);
  void removePack(const char *pack);
  // QString listPackages();

  void forceCheckoutMaster(const char *pack);

  // 从数据库读取所有包。将repo的实际HEAD写入到db
  // 适用于自己手动git pull包后使用
  void syncCommitHashToDatabase();

private:
  std::unique_ptr<Sqlite3> db;
  std::vector<std::string> disabled_packs;

  int clone(const char *url);
  int pull(const char *name);
  int checkout(const char *name, const char *hash);
  int checkout_branch(const char *name, const char *branch);
  int status(const char *name); // return 1 if the workdir is modified
  std::string head(const char *name); // get commit hash of HEAD
};

extern PackMan *Pacman;

#endif
