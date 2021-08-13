#ifndef _MENU_H_
#define _MENU_H_

#define KEY_INITIALIZING 6
#define MENU_RESULT_EXIT -2

#include <Arduino.h>
#include <Ucglib.h> //http://librarymanager/All#Ucglib
#include "buttons.h"

class MenuItem
{
  public:
    MenuItem(const int id, const char* title, const char* value = nullptr, const uint32_t tag = 0)
     : _id(id), _title(title), _childCount(0), _tag(tag)
    {
      _children = nullptr;
      _parent = nullptr;
      _value = value;
    };
    MenuItem(const int id, const char* title, MenuItem** children,  const uint8_t childCount)
     : _id(id), _title(title), _childCount(childCount), _tag(0)
    {
      _children = children;
      _parent = nullptr;
      for(uint8_t i=0;i<childCount;i++)
        _children[i]->setParent(this);
    };
    const char* getTitle() { return _title; };
    MenuItem* getParent() { return _parent; };
    MenuItem* getChild(uint8_t i) { return _children[i]; };
    const uint8_t getChildCount() { return _childCount; };
    const int getId() { return _id; };
    String getValue() { return _value; };
    const uint32_t getTag() { return _tag; };
    void setValue(const char *value) { _value = String(value); };
    void setValue(String value) { _value = value; };
    void setChildren(MenuItem** children,  const uint8_t childCount) { _children = children, _childCount = childCount; };
    void setTitle(const char* title) { _title = title; };
  private:
    void setParent(MenuItem* parent) { _parent=parent; }
    const int _id;
    const char* _title;
    String _value;
    uint8_t _childCount;
    const uint32_t _tag;
    MenuItem** _children;
    MenuItem* _parent;
};

class Menu
{
  public:
    Menu(MenuItem* menuItem);
    int processMenu();
    void initScreen();
    void setDisplay(Ucglib *display) { _ucg = display; };
    void reset() { currentMenu = rootMenu; clearMenuIndex = -1; selectedIndex = 0; };
    MenuItem* selectedMenuItem() { return currentMenu->getChild(selectedIndex); };
    void refresh() { show(); };

  private:
    void show();
    void up();
    void down();
    bool left();
    int select();
    void drawHeader();
    //state
    uint8_t selectedIndex;
    MenuItem* rootMenu;
    MenuItem* currentMenu;
    int currentButton = KEY_NONE;    
    int clearMenuIndex = -1;
    // display settings
    uint8_t yOffset = 10;
    uint8_t yPadding = 3;
    uint8_t titleHeight = 18;
    uint8_t screen_width = 128;
    uint8_t screen_height = 128;
    uint8_t scrollOffset = 0;
    Ucglib *_ucg;
};
#endif
