#ifndef _MENU_H_
#define _MENU_H_

#define KEY_NONE 0
#define KEY_LEFT 1
#define KEY_RIGHT 2
#define KEY_UP 3
#define KEY_DOWN 4
#define KEY_SELECT 5
#define KEY_INITIALIZING 6
#define MENU_RESULT_EXIT -2

#include <Arduino.h>
#include <Ucglib.h> //http://librarymanager/All#Ucglib
class MenuItem
{
  public:
    MenuItem(const int id, const char* title, const char* value = nullptr)
     : _id(id), _title(title), _childCount(0)
    {
      _children = nullptr;
      _parent = nullptr;
      _value = value;
    };
    MenuItem(const int id, const char* title, MenuItem** children,  const uint8_t childCount)
     : _id(id), _title(title), _childCount(childCount)
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
    const char* getValue() { return _value; };
    void setValue(const char* value) { _value = value; };
  private:
    void setParent(MenuItem* parent) { _parent=parent; }
    const int _id;
    const char* _title;
    const char* _value;
    const uint8_t _childCount;
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
    void right();
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
    // Button pin IDs
    uint8_t uiKeyUp = 6;
    uint8_t uiKeySelect = 11;
    uint8_t uiKeyLeft = 5;
    uint8_t uiKeyDown = 10;
    uint8_t uiKeyRight = 9;
};
#endif
