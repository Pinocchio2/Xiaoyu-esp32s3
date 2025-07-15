#ifndef DUAL_DISPLAY_MANAGER_H
#define DUAL_DISPLAY_MANAGER_H

#include "display/lcd_display.h"
#include "config.h"

class DualDisplayManager {
private:
    LcdDisplay* primary_display_;
    LcdDisplay* secondary_display_;
    lv_obj_t* primary_img_obj_;   // 新增：主屏幕的图像对象
    lv_obj_t* secondary_img_obj_; // 新增：副屏幕的图像对象
    SpiLcdDisplay* CreateSpiLcdDisplayWithoutInit(
    esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts);
public:
    DualDisplayManager();
    ~DualDisplayManager();
    
    void Initialize();
    void InitializeUI(); // 新增UI初始化方法
    void SetImage(bool is_primary, const void* src);
    
    // 获取显示屏
    Display* GetPrimaryDisplay() { return primary_display_; }
    Display* GetSecondaryDisplay() { return secondary_display_; }
   
    
    // 双屏控制方法
    void ShowOnBoth(const char* message);
    void ShowOnPrimary(const char* message);
    void ShowOnSecondary(const char* message);
    
    // 分屏显示不同内容
    void SetDifferentContent(const char* primary_content, const char* secondary_content);
    
    // 镜像显示
    void SetMirrorMode(bool enable);

    // 简单验证测试
    void TestDifferentContent();   // 测试双屏显示不同内
};

#endif