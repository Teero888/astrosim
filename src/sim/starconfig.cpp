#include "../gfx/graphics.h"
#include "body.h"
#include "starsystem.h"

#include <toml.hpp>

#include <iostream>
#include <string>

#include "glm/ext/vector_float3.hpp"

// Helper to get vec3 from array
static glm::vec3 get_vec3(const toml::array *arr)
{
	if(!arr || arr->size() < 3)
		return glm::vec3(0.0f);
	return glm::vec3(
		arr->get(0)->value_or(0.0f),
		arr->get(1)->value_or(0.0f),
		arr->get(2)->value_or(0.0f));
}

static Vec3 get_Vec3(const toml::array *arr)
{
	if(!arr || arr->size() < 3)
		return Vec3(0.0);
	return Vec3(
		arr->get(0)->value_or(0.0),
		arr->get(1)->value_or(0.0),
		arr->get(2)->value_or(0.0));
}

void CStarSystem::LoadBodies(const std::string &Filename)
{
	m_vBodies.clear();

	toml::table tbl;
	try
	{
		tbl = toml::parse_file(Filename);
	}
	catch(const toml::parse_error &err)
	{
		std::cerr << "Error parsing TOML: " << err << "\n";
		m_vBodies.emplace_back(0, "Error", SBody::SSimParams{1, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0)}, SBody::SRenderParams{1, glm::vec3(1, 0, 0)});
		return;
	}

	if(!tbl["bodies"].is_array())
	{
		std::cerr << "Error: 'bodies' array not found in config.\n";
		m_vBodies.emplace_back(0, "Error", SBody::SSimParams{1, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0)}, SBody::SRenderParams{1, glm::vec3(1, 0, 0)});
		return;
	}

	const auto &bodies = *tbl["bodies"].as_array();
	int IdCounter = 0;

	for(const auto &elem : bodies)
	{
		if(!elem.is_table())
			continue;
		const auto &bodyTbl = *elem.as_table();

		std::string Name = bodyTbl["name"].value_or("Unknown");

		SBody::SSimParams SimParams = {};
		SBody::SRenderParams RenderParams = {};

		// Type
		std::string typeStr = bodyTbl["type"].value_or("TERRESTRIAL");
		if(typeStr == "STAR")
			RenderParams.m_BodyType = EBodyType::STAR;
		else if(typeStr == "GAS_GIANT")
			RenderParams.m_BodyType = EBodyType::GAS_GIANT;
		else
			RenderParams.m_BodyType = EBodyType::TERRESTRIAL;

		// Terrain Type
		std::string terrainTypeStr = bodyTbl["terrain_type"].value_or("terrestrial");
		if(terrainTypeStr == "volcanic")
			RenderParams.m_TerrainType = ETerrainType::VOLCANIC;
		else if(terrainTypeStr == "ice")
			RenderParams.m_TerrainType = ETerrainType::ICE;
		else if(terrainTypeStr == "barren")
			RenderParams.m_TerrainType = ETerrainType::BARREN;
		else
			RenderParams.m_TerrainType = ETerrainType::TERRESTRIAL;

		SimParams.m_Mass = bodyTbl["mass"].value_or(1.0);
		RenderParams.m_Radius = bodyTbl["radius"].value_or(1.0);
		RenderParams.m_Seed = bodyTbl["seed"].value_or(0);
		RenderParams.m_RotationPeriod = bodyTbl["rotation_period"].value_or(0.0);
		RenderParams.m_Obliquity = bodyTbl["obliquity"].value_or(0.0);

		SimParams.m_Position = get_Vec3(bodyTbl["position"].as_array());
		SimParams.m_Velocity = get_Vec3(bodyTbl["velocity"].as_array());
		RenderParams.m_Color = get_vec3(bodyTbl["color"].as_array());

		// Colors
		if(const auto *colors = bodyTbl["colors"].as_table())
		{
			RenderParams.m_Colors.m_DeepOcean = get_vec3((*colors)["deep_ocean"].as_array());
			RenderParams.m_Colors.m_ShallowOcean = get_vec3((*colors)["shallow_ocean"].as_array());
			RenderParams.m_Colors.m_Beach = get_vec3((*colors)["beach"].as_array());
			RenderParams.m_Colors.m_Grass = get_vec3((*colors)["grass"].as_array());
			RenderParams.m_Colors.m_Forest = get_vec3((*colors)["forest"].as_array());
			RenderParams.m_Colors.m_Desert = get_vec3((*colors)["desert"].as_array());
			RenderParams.m_Colors.m_Snow = get_vec3((*colors)["snow"].as_array());
			RenderParams.m_Colors.m_Rock = get_vec3((*colors)["rock"].as_array());
			RenderParams.m_Colors.m_Tundra = get_vec3((*colors)["tundra"].as_array());
		}

		// Terrain
		if(const auto *terrain = bodyTbl["terrain"].as_table())
		{
			RenderParams.m_Terrain.m_ContinentFrequency = (*terrain)["continent_frequency"].value_or(0.02f);
			RenderParams.m_Terrain.m_ContinentOctaves = (*terrain)["continent_octaves"].value_or(4);
			RenderParams.m_Terrain.m_ContinentHeight = (*terrain)["continent_height"].value_or(0.005f);
			RenderParams.m_Terrain.m_SeaLevel = (*terrain)["sea_level"].value_or(0.0f);
			RenderParams.m_Terrain.m_OceanDepth = (*terrain)["ocean_depth"].value_or(0.01f);

			RenderParams.m_Terrain.m_MountainFrequency = (*terrain)["mountain_frequency"].value_or(0.08f);
			RenderParams.m_Terrain.m_MountainOctaves = (*terrain)["mountain_octaves"].value_or(6);
			RenderParams.m_Terrain.m_MountainHeight = (*terrain)["mountain_height"].value_or(0.015f);
			RenderParams.m_Terrain.m_MountainMaskFrequency = (*terrain)["mountain_mask_frequency"].value_or(0.01f);
			RenderParams.m_Terrain.m_MountainWarpStrength = (*terrain)["mountain_warp_strength"].value_or(1.0f);

			RenderParams.m_Terrain.m_HillsFrequency = (*terrain)["hills_frequency"].value_or(0.3f);
			RenderParams.m_Terrain.m_HillsOctaves = (*terrain)["hills_octaves"].value_or(3);
			RenderParams.m_Terrain.m_HillsHeight = (*terrain)["hills_height"].value_or(0.001f);

			RenderParams.m_Terrain.m_DetailFrequency = (*terrain)["detail_frequency"].value_or(1.5f);
			RenderParams.m_Terrain.m_DetailOctaves = (*terrain)["detail_octaves"].value_or(4);
			RenderParams.m_Terrain.m_DetailHeight = (*terrain)["detail_height"].value_or(0.0005f);

			RenderParams.m_Terrain.m_PolarIceCapLatitude = (*terrain)["polar_ice_cap_latitude"].value_or(0.75f);
			RenderParams.m_Terrain.m_MoistureOffset = (*terrain)["moisture_offset"].value_or(0.0f);
			RenderParams.m_Terrain.m_TemperatureOffset = (*terrain)["temperature_offset"].value_or(0.0f);
		}

		// Gas Giant
		if(const auto *gg = bodyTbl["gasgiant"].as_table())
		{
			RenderParams.m_GasGiant.m_BaseColor = get_vec3((*gg)["base_color"].as_array());
			RenderParams.m_GasGiant.m_BandColor = get_vec3((*gg)["band_color"].as_array());
			RenderParams.m_GasGiant.m_WindSpeed = (*gg)["wind_speed"].value_or(1.0f);
			RenderParams.m_GasGiant.m_Turbulence = (*gg)["turbulence"].value_or(1.0f);
			RenderParams.m_GasGiant.m_Seed = (*gg)["seed"].value_or(0.0f);
		}

		// Atmosphere
		if(const auto *atm = bodyTbl["atmosphere"].as_table())
		{
			RenderParams.m_Atmosphere.m_Enabled = (*atm)["enabled"].value_or(false);
			RenderParams.m_Atmosphere.m_AtmosphereRadius = (*atm)["radius"].value_or(1.025f);
			RenderParams.m_Atmosphere.m_SunIntensity = (*atm)["sun_intensity"].value_or(20.0f);
			RenderParams.m_Atmosphere.m_RayleighScatteringCoeff = get_vec3((*atm)["rayleigh_scattering_coeff"].as_array());
			RenderParams.m_Atmosphere.m_RayleighScaleHeight = (*atm)["rayleigh_scale_height"].value_or(8000.0f);
			RenderParams.m_Atmosphere.m_MieScatteringCoeff = get_vec3((*atm)["mie_scattering_coeff"].as_array());
			RenderParams.m_Atmosphere.m_MieScaleHeight = (*atm)["mie_scale_height"].value_or(1200.0f);
			RenderParams.m_Atmosphere.m_MiePreferredScatteringDir = (*atm)["mie_preferred_scattering_dir"].value_or(0.758f);
		}

		// Rotation Logic
		if(RenderParams.m_RotationPeriod != 0.0)
		{
			Vec3 orbitalUp(0, 1, 0);
			Quat tilt = Quat::FromAxisAngle(Vec3(0, 0, 1), -RenderParams.m_Obliquity);
			Vec3 rotationAxis = tilt.RotateVector(orbitalUp);
			SimParams.m_Orientation = tilt;
			double angSpeed = (2.0 * PI) / RenderParams.m_RotationPeriod;
			SimParams.m_AngularVelocity = rotationAxis * angSpeed;
		}

		m_vBodies.emplace_back(IdCounter++, Name, SimParams, RenderParams);
	}

	if(!m_vBodies.empty())
	{
		m_pSunBody = nullptr;
		for(auto &body : m_vBodies)
		{
			if(body.m_RenderParams.m_BodyType == EBodyType::STAR)
			{
				m_pSunBody = &body;
				break;
			}
		}
		if(!m_pSunBody)
			m_pSunBody = &m_vBodies.front();
	}
	else
		m_pSunBody = nullptr;
}
