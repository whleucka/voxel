#pragma once

#include "journal/journal.hpp"

class Hud;

// The Journal book (J): a Notes tab and a Collections tab, drawn as an ImGui
// window over the game. It remembers which tab and page were open.
class JournalView {
public:
  enum class Tab { Notes, Collections };

  // Draws the book; true when its Close button was pressed.
  bool draw(const Hud &hud, const SeenItems &seen, const JournalProgress &journal,
            bool creative);

  Tab tab() const { return tab_; }
  JournalPage page() const { return page_; }

private:
  Tab tab_ = Tab::Notes;
  JournalPage page_ = JournalPage::Creatures;

  void drawNotes(const SeenItems &seen, const JournalProgress &journal);
  void drawCollections(const Hud &hud, const SeenItems &seen, const JournalProgress &journal);
};
