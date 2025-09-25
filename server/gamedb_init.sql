-- SPDX-License-Identifier: GPL-3.0-or-later

CREATE TABLE IF NOT EXISTS gameSaves (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  uid INTEGER NOT NULL,
  mode TEXT NOT NULL,
  data BLOB NOT NULL,
  UNIQUE(uid, mode)
);

CREATE INDEX IF NOT EXISTS idx_gameSaves_uid ON gameSaves(uid);
CREATE INDEX IF NOT EXISTS idx_gameSaves_mode ON gameSaves(mode);