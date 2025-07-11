#pragma once
#include <stdint.h>
#include "esp_timer.h"

// 眼睛参数结构体，定义了眼睛的所有外观属性
struct EyeConfig {
	int16_t OffsetX;
	int16_t OffsetY;
 	int16_t Height;
	int16_t Width;
	float Slope_Top;
	float Slope_Bottom;
	int16_t Radius_Top;
	int16_t Radius_Bottom;
};

// “正常”和“开心”两个预设表情
static const EyeConfig Preset_Normal = {
	.OffsetX = 0, .OffsetY = 0, .Height = 40, .Width = 40,
	.Slope_Top = 0, .Slope_Bottom = 0, .Radius_Top = 15, .Radius_Bottom = 15,
};

static const EyeConfig Preset_Happy = {
	.OffsetX = 0, .OffsetY = 0, .Height = 8, .Width = 42,
	.Slope_Top = -0.4, .Slope_Bottom = -0.4, .Radius_Top = 5, .Radius_Bottom = 5,
};

// 简单的“斜坡”动画，在指定时间内从0.0线性过渡到1.0
class RampAnimation {
public:
	unsigned long Interval;
	unsigned long StarTime;

	RampAnimation(unsigned long interval) : Interval(interval), StarTime(0) {}

	void Restart() {
		StarTime = esp_timer_get_time() / 1000;
	}

	float GetValue() {
        if (StarTime == 0) return 0.0f; // 动画未启动
		unsigned long elapsedMillis = (esp_timer_get_time() / 1000) - StarTime;
		if (elapsedMillis >= Interval) {
            StarTime = 0; // 动画结束
			return 1.0f;
		}
		return static_cast<float>(elapsedMillis) / Interval;
	}
};

// 核心的过渡控制器
class EyeTransition {
public:
	EyeTransition() : Animation(300) {} // 动画持续300毫秒

	EyeConfig* Origin = nullptr; // 指向当前状态的指针
	EyeConfig Destin;            // 目标状态
	RampAnimation Animation;

	// 更新函数：根据时间进度，计算出当前帧的眼睛参数
	void Update() {
        if (Origin == nullptr) return;
		float t = Animation.GetValue();
		Origin->OffsetX = Origin->OffsetX * (1.0 - t) + Destin.OffsetX * t;
		Origin->OffsetY = Origin->OffsetY * (1.0 - t) + Destin.OffsetY * t;
		Origin->Height = Origin->Height * (1.0 - t) + Destin.Height * t;
		Origin->Width = Origin->Width * (1.0 - t) + Destin.Width * t;
		Origin->Slope_Top = Origin->Slope_Top * (1.0 - t) + Destin.Slope_Top * t;
		Origin->Slope_Bottom = Origin->Slope_Bottom * (1.0 - t) + Destin.Slope_Bottom * t;
		Origin->Radius_Top = Origin->Radius_Top * (1.0 - t) + Destin.Radius_Top * t;
		Origin->Radius_Bottom = Origin->Radius_Bottom * (1.0 - t) + Destin.Radius_Bottom * t;
	}
};