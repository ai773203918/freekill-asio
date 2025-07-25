#include "c-wrapper.h"
#include <sqlite3.h>
#include <spdlog/spdlog.h>

Sqlite3::Sqlite3(const char *filename, const char *initSql) {
  std::ifstream file { initSql, std::ios_base::in };
  if (!file.is_open()) {
    spdlog::error("cannot open %s. Quit now.", initSql);
    std::exit(1);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  auto sql = buffer.str();

  int rc;
  char *err_msg;

  rc = sqlite3_open(filename, &db);
  if (rc != SQLITE_OK) {
    spdlog::critical("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    std::exit(1);
  }

  rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    spdlog::critical("sqlite error: %s", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    std::exit(1);
  }
}

Sqlite3::~Sqlite3() {
  sqlite3_close(db);
}

bool Sqlite3::checkString(const char *str) {
  static const std::regex exp(R"(['\";#* /\\?<>|:]+|(--)|(/\*)|(\*/)|(--\+))");
  return !std::regex_search(str, exp);
}

// callback for handling SELECT expression
static int callback(void *jsonDoc, int argc, char **argv, char **cols) {
  QMap<QString, QString> obj;
  for (int i = 0; i < argc; i++) {
    obj[cols[i]] = argv[i] ? argv[i] : "#null";
  }
  ((Sqlite3::QueryResult *)jsonDoc)->append(obj);
  return 0;
}

Sqlite3::QueryResult Sqlite3::select(const char *sql) {
  static QMutex select_lock;
  QueryResult arr;
  char *err = NULL;
  auto bytes = sql.toUtf8();
  QMutexLocker locker(&select_lock);
  sqlite3_exec(db, bytes.data(), callback, (void *)&arr, &err);
  if (err) {
    qCritical() << err;
    sqlite3_free(err);
  }
  return arr;
}

QString Sqlite3::selectJson(const char *sql) {
  auto ret = select(sql);
  QJsonArray arr;
  for (auto map : ret) {
    QJsonObject obj;
    for (auto i = map.cbegin(), end = map.cend(); i != end; i++) {
      obj[i.key()] = i.value();
    }
    arr.append(obj);
  }
  return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

void Sqlite3::exec(const std::string &sql) {
  auto bytes = sql.c_str();
  sqlite3_exec(db, bytes, nullptr, nullptr, nullptr);
}

std::uint64_t Sqlite3::getMemUsage() {
  return sqlite3_memory_used();
}
