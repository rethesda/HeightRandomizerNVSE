#include "main.h"

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "HeightRandomizer";
	info->version = 1;

	if (nvse->isEditor)
	{
		return false;
	}
	gLog.Create("HeightRandomizer.log");
	if (nvse->runtimeVersion != RUNTIME_VERSION_1_4_0_525)
	{
		PrintLog("ERROR: Unsupported runtime version (%08X).", nvse->runtimeVersion);
		return false;
	}
	PrintLog("successful load");
	return true;
}

constexpr DWORD playerRefID = 0x14, playerBaseId = 0x7;

float (*CurveModifier)(unsigned int, unsigned int);
unsigned int (*NumberManipulatorFunction)(unsigned int);

float g_uSPECIALPercentage;
float g_uMaxDriftPercentage;
float g_uMaxDriftPercentageCreatures;

float g_uFemalePercentage;
unsigned int g_iRandomizerSeed;
int g_iRandomizerMode = 1;
enum RandomizerMode {
	kNone = 0,
	kNPC = 1 << 0,
	kCreature = 1 << 1


};

float __fastcall HeightRandomizerHook(TESObjectREFR* form)
{
	float returnscale = form->scale;
	auto refrBase = form->baseForm;

	auto applyActorScaleModifiers = [form, refrBase] (float& returnscale, float driftPerc) -> bool {

		TESActorBase* baseActor = (TESActorBase*)refrBase;
		if (fabs(returnscale - 1) < 0.009 && form->refID != playerRefID && refrBase->refID != playerBaseId && CurveModifier && NumberManipulatorFunction)
		{
			returnscale += (CurveModifier(NumberManipulatorFunction(form->refID & 0xFFFFFF), g_iRandomizerSeed) * (driftPerc * (1 - g_uSPECIALPercentage)));
			returnscale += (((baseActor->attributes.attributes[TESAttributes::kStrength] - baseActor->attributes.attributes[TESAttributes::kAgility]) / 2 + (baseActor->attributes.attributes[TESAttributes::kEndurance] - 5.5) + (baseActor->attributes.attributes[TESAttributes::kLuck] - 5.5) / 4.5)) / 10 * (g_uMaxDriftPercentage * (g_uSPECIALPercentage));
			return true;
		}
		return false;
	};

	if (refrBase) {
		if (form->baseForm->typeID == kFormType_Creature)
		{	
			TESCreature* baseCreature = ((TESCreature*)refrBase);
			returnscale *= baseCreature->baseScale;
			if ((g_iRandomizerMode & kCreature) && baseCreature->type <= 5) //exclude robots and giants by default
			{
				applyActorScaleModifiers(returnscale, g_uMaxDriftPercentageCreatures);
			}

		}
		else if (form->baseForm->typeID == kFormType_NPC)
		{
			TESNPC* baseNPC = ((TESNPC*)refrBase);
			returnscale *= baseNPC->height;
			
			if (applyActorScaleModifiers(returnscale, g_uMaxDriftPercentage))
			{
				if (baseNPC->baseData.flags & TESActorBaseData::kFlags_Female) returnscale *= g_uFemalePercentage;
			}
		}
	
	}
	return returnscale;
}
bool NVSEPlugin_Load(const NVSEInterface* nvse)
{
	char iniDir[MAX_PATH];

	GetModuleFileNameA(GetModuleHandle(NULL), iniDir, MAX_PATH);
	g_iRandomizerMode = GetPrivateProfileInt("Main", "iRandomizerMode", 1, iniDir);
	if (!g_iRandomizerMode) return true;
	if ((unsigned int(GetPrivateProfileInt("Main", "uHeightAlgorithm", 1, iniDir))) == 0) return true;
	strcpy((char*)(strrchr(iniDir, '\\') + 1), "Data\\NVSE\\Plugins\\HeightRandomizer.ini");
	PrintLog("%s", iniDir);
	unsigned int intArgtmp;
	intArgtmp = GetPrivateProfileInt("Main", "uSPECIALPercentage", 15, iniDir) ;
	if (intArgtmp >= 100) intArgtmp = 100;
	g_uSPECIALPercentage = float(intArgtmp) / 100;
	intArgtmp = GetPrivateProfileInt("Main", "uMaxDriftPercentage", 15, iniDir);
	if (intArgtmp >= 99) intArgtmp = 99;
	g_uMaxDriftPercentage = float(intArgtmp) / 50;
	//Creatures
	intArgtmp = GetPrivateProfileInt("Main", "uMaxDriftPercentageCreature", 0, iniDir);
	if (intArgtmp >= 99) intArgtmp = 99;
	if (intArgtmp <= 0) {
		intArgtmp = g_uMaxDriftPercentage;
	}
	g_uMaxDriftPercentageCreatures = float(intArgtmp) / 50;

	//etc
	g_uFemalePercentage = ((float) GetPrivateProfileInt("Main", "uFemalePercentage", 92, iniDir)) / 100;
	g_iRandomizerSeed = GetPrivateProfileIntA("Main", "iRandomizerSeed", 0, iniDir);
	if (g_iRandomizerSeed <= 0) {
		g_iRandomizerSeed = 101;
	}

	if (!GetPrivateProfileIntA("Main", "bUseTrueRandom", 0, iniDir)) {
		NumberManipulatorFunction = HeightRandomizer::FNV1aHasher;
	}
	else {
		NumberManipulatorFunction = HeightRandomizer::RandomizerDefault;
	}
	switch (unsigned int(GetPrivateProfileIntA("Main", "uHeightAlgorithm", 1, iniDir)) % 3) {
	case 1:
		CurveModifier = HeightRandomizer::CurveNormalizerRealistic;
		break;
	case 2:
		CurveModifier = HeightRandomizer::CurveNormalizerHighVariance;
		break;
	default:
		CurveModifier = HeightRandomizer::CurveNormalizerSemilinear;
	}

	if (g_uFemalePercentage <= 0.1) { g_uFemalePercentage = 0.1; }
	WriteRelJump(0x0567400, (UInt32)HeightRandomizerHook);

	return true;
}







