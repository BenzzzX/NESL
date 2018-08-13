#pragma once

#include "../NESL/TbbGraph.h"
#include "../NESL/VisualGraph.h"
#include <Windows.h>


#pragma region States

using GEntities = ESL::Entities;

struct GCanvas { HANDLE handle; };
GLOBAL_STATE(GCanvas);

struct EAppearance { char v; }; //显示的字符
ENTITY_STATE(EAppearance, Vec);

struct ELocation { int32_t x, y; }; //位置
ENTITY_STATE(ELocation, Vec);

struct EVelocity { int32_t x, y; }; //移动速度
ENTITY_STATE(EVelocity, Vec);

struct ELifeTime { int32_t n; }; //计时死亡
ENTITY_STATE(ELifeTime, Vec);

struct ELength { int32_t life; }; //生成计时死亡的实体
ENTITY_STATE(ELength, Vec);

#pragma endregion


void Logic_Spawn(const ELength& sp, const ELocation& loc, GEntities& entities,
	ESL::State<ELifeTime>& lifetimes, ESL::State<ELocation>& locations,
	ESL::State<EAppearance>& appearances);

void Logic_LifeTime(ELifeTime& life, ESL::Entity self, GEntities& entities);

void Logic_Move(ELocation& loc, EVelocity& vel);

void Logic_Draw(const ELocation& loc, const EAppearance& ap, GCanvas& canvas);

void Logic_Clear(const ELifeTime& life, EAppearance& ap);