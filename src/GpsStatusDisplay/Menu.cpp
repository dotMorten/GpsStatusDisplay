#include "Menu.h"

Menu::Menu(MenuItem* menuItem)
{
  rootMenu = menuItem; 
  currentMenu = menuItem;
  clearMenuIndex = -1;
  selectedIndex = 0; 
}
    
void Menu::initScreen()
{
  drawHeader();
  currentButton = KEY_NONE;
  show(); 
  if (getButtonState() == KEY_LEFT) // Ensure select isn't triggered when launching if already pressed
    currentButton = KEY_INITIALIZING;
}
void Menu::drawHeader()
{
  auto ucg = _ucg;
  ucg->setFont(ucg_font_helvB10_hr);
  auto ascent = ucg->getFontAscent();
  auto descent = -ucg->getFontDescent();
  auto width = ucg->getStrWidth(currentMenu->getTitle());  
  // Draw title background
  ucg->setColor(100, 129, 237);
  ucg->drawBox(0, yOffset, screen_width, titleHeight);
  // Draw title centered on background
  ucg->setFontMode(UCG_FONT_MODE_TRANSPARENT);
  ucg->setColor(255, 255, 255);
  ucg->drawString(screen_width / 2 - width / 2, yOffset + (titleHeight - ascent) / 2 + ascent + 1, 0, currentMenu->getTitle());

  ucg->setColor(0, 0, 0);
  ucg->drawBox(0, yOffset + titleHeight, screen_width, screen_height - yOffset - titleHeight);
}
void Menu::show() 
{
  auto ucg = _ucg;
  ucg->setFont(ucg_font_helvR10_hr);
  auto ascent = ucg->getFontAscent();
  auto descent = -ucg->getFontDescent();
 
  int count = currentMenu->getChildCount();
  int maxrows = (screen_height - yOffset - titleHeight + yPadding) / (ascent + descent + yPadding);  
  Serial.println(("max rows:" + String(maxrows)).c_str());
  bool scrollRequired = false;
  if(selectedIndex < scrollOffset)
  {
    scrollOffset = selectedIndex;
    scrollRequired = true;
  }
  else if(selectedIndex - scrollOffset >= maxrows)
  {
    scrollOffset = selectedIndex - maxrows + 1;    
    scrollRequired = true;
  }
  Serial.println(("scrollOffset:" + String(scrollOffset)).c_str());
  Serial.println(("selectedIndex:" + String(selectedIndex)).c_str());
  if(scrollRequired)
  {
    //We need to scroll, so clear menu area
    ucg->setColor(0, 0, 0);
    ucg->drawBox(0, yOffset + titleHeight, screen_width, screen_height - yOffset - titleHeight);
    clearMenuIndex = -1;    
  }
  int y = yOffset + titleHeight;
  for(uint8_t i = 0; i < count; i++)
  {
    if(i < scrollOffset || i-scrollOffset >= maxrows) continue; //skip rows outside screen
    y += ascent+descent;
    if(i == clearMenuIndex)
    {
      ucg->setColor(0, 0, 0);
      ucg->drawBox(0, y-ascent-3, screen_width, ascent+6);
      clearMenuIndex = -1;
    }
    ucg->setColor(255,255,255);
    if(i == selectedIndex)
    {
       // Invert
      if(currentButton == KEY_RIGHT)
      {
        // invert to selection color while button is pressed
        ucg->setColor(255,255,0);
      }
      else {
        ucg->setColor(40,40,40);
      }
      ucg->drawBox(0, y-ascent-3, screen_width, ascent+6);
      ucg->setFontMode(UCG_FONT_MODE_TRANSPARENT);
      ucg->setColor(128, 128, 255);
    }
    else {
      ucg->setFontMode(UCG_FONT_MODE_SOLID);
    }
    MenuItem* item = currentMenu->getChild(i);
    const char* text = item->getTitle();
    ucg->drawString(2, y, 0, text);
    if(item->getChildCount() > 0)
      ucg->drawString(120, y, 0, ">");
    else {
      auto value = item->getValue();
      if(value != nullptr)
      {
        auto width = ucg->getStrWidth(value.c_str());
        ucg->drawString(screen_width - width - 1, y, 0, value.c_str());
      }
    }
    y += yPadding;
  }
}

void Menu::up()
{ 
  clearMenuIndex = selectedIndex;
  if(selectedIndex == 0)
    selectedIndex = currentMenu->getChildCount() - 1;
  else
    selectedIndex--;
  show();
}

void Menu::down()
{
  clearMenuIndex = selectedIndex++;
  if(selectedIndex >= currentMenu->getChildCount())
    selectedIndex = 0;
  show();
}

bool Menu::left()
{
  auto parent = currentMenu->getParent();
  if(parent != nullptr)
  {
    auto childmenu = currentMenu;
    currentMenu = parent;
    for(int i = 0; i<currentMenu->getChildCount(); i++)
    {
      if(currentMenu->getChild(i) == childmenu)
      {
        selectedIndex = i;
        break;
      }
    }
    drawHeader();
    show();
    return true;
  }
  return false;
}

int Menu::select()
{ 
  auto item = selectedMenuItem();
  if(item->getChildCount() > 0)
  {
    selectedIndex = 0;
    currentMenu = item;
    drawHeader();
  }
  show();
  return item->getId();
}

int Menu::processMenu()
{
  int result = -1;
  int button = getButtonState();
  if(button == KEY_LEFT && currentButton == KEY_INITIALIZING)
    button = KEY_INITIALIZING;

  auto oldButton = currentButton;
  currentButton = button;
  if (oldButton != currentButton)
  {
    Serial.println(("Button pressed: " + String(button)).c_str());
    // Act on button change
    if(button == KEY_UP)
      up();
    else if(button == KEY_DOWN)
      down();    
    else if(button == KEY_LEFT)
    {
      if(!left())
        result = MENU_RESULT_EXIT;
    }
    else if(oldButton == KEY_RIGHT && button == KEY_NONE)
    {
       //Select item on release
       result = select();
    }
    else if(button == KEY_RIGHT)
    {
       // Do nothing, but show() will draw item with selection color
       // We'll act on select on release below
       show();
    }
  }
  return result;
}
