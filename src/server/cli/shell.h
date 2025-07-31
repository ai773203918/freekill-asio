// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SHELL_H
#define _SHELL_H

class Shell {
public:
  typedef std::vector<std::string> StringList;
  Shell();
  Shell(Shell &) = delete;
  Shell(Shell &&) = delete;
  ~Shell();

  void start();
  void handleLine(char *);

protected:
  virtual void run();

private:
  std::thread m_thread;

  bool done = false;
  std::unordered_map<std::string_view, void (Shell::*)(StringList &)> handler_map;
  void helpCommand(StringList &);
  void quitCommand(StringList &);
  void lspCommand(StringList &);
  void lsrCommand(StringList &);
  void installCommand(StringList &);
  void removeCommand(StringList &);
  void upgradeCommand(StringList &);
  void lspkgCommand(StringList &);
  void syncpkgCommand(StringList &);
  void enableCommand(StringList &);
  void disableCommand(StringList &);
  void kickCommand(StringList &);
  void msgCommand(StringList &);
  void msgRoomCommand(StringList &);
  void banCommand(StringList &);
  void banipCommand(StringList &);
  void banUuidCommand(StringList &);
  void unbanCommand(StringList &);
  void unbanipCommand(StringList &);
  void unbanUuidCommand(StringList &);
  void reloadConfCommand(StringList &);
  void resetPasswordCommand(StringList &);
  void statCommand(StringList &);
  void killRoomCommand(StringList &);

private:
  // QString syntaxHighlight(char *);
public:
  void redisplay();
  void moveCursorToStart();
  void clearLine();
  bool lineDone() const;
  char *generateCommand(const char *, int);
};

#endif
