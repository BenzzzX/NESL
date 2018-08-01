#pragma once

#include "../NESL/NESL.h"


#pragma region States

using GEntities = ESL::Entities;

struct GFrame { uint64_t total; };
GLOBAL_STATE(GFrame);

struct GCanvas { };
GLOBAL_STATE(GCanvas);

struct EAppearance { char v; }; //��ʾ���ַ�
ENTITY_STATE(EAppearance, Vec);

struct ELocation { int32_t x, y; }; //λ��
ENTITY_STATE(ELocation, Vec);

struct EVelocity { int32_t x, y; }; //�ƶ��ٶ�
ENTITY_STATE(EVelocity, Vec);

struct ELifeTime { int32_t n; }; //��ʱ����
ENTITY_STATE(ELifeTime, Vec);

struct ESpawner { int32_t life; }; //���ɼ�ʱ������ʵ��
ENTITY_STATE(ESpawner, Hash);

#pragma endregion


auto Logic_Spawn(const ESpawner& sp, const ELocation& loc, GEntities& entities,
	ESL::State<ELifeTime>& lifetimes, ESL::State<ELocation>& locations,
	ESL::State<EAppearance>& appearances);

auto Logic_LifeTime(ELifeTime& life, ESL::Entity self, const GEntities& entities);

auto Logic_Move(ELocation& loc, EVelocity& vel, const GCanvas& canvas);

auto Logic_Draw(const ELocation& loc, const EAppearance& ap, const GCanvas& canvas);