namespace
{
	bool skipSolstheim = true;
	bool keepDarkColors = false;
	int keepDarkerThan = 8;
	bool onlyListed = false;
	std::vector<std::string> listedNames;
	std::vector<const RE::TESLandTexture*> listedTextures;

	bool IsListed(const RE::TESLandTexture* a_texture)
	{
		return !onlyListed || std::ranges::contains(listedTextures, a_texture);
	}

	void Recolor(RE::TESObjectLAND::LoadedLandData* a_data)
	{
		for (std::size_t quad = 0; quad < 4; ++quad) {
			for (std::size_t vertex = 0; vertex < 289; ++vertex) {
				auto& color = a_data->colors[quad][vertex];
				const int brightest = std::max({ static_cast<std::uint8_t>(color[0]), static_cast<std::uint8_t>(color[1]), static_cast<std::uint8_t>(color[2]) });
				if (brightest == 0 || (keepDarkColors && brightest < keepDarkerThan)) {
					continue;
				}
				const auto gridX = (quad & 1) * 16 + vertex % 17;
				const auto gridY = (quad >> 1) * 16 + vertex / 17;
				int layers = 0;
				int listed = 0;
				bool listedBase = false;
				for (std::size_t source = 0; source < 4; ++source) {
					const auto localX = gridX - (source & 1) * 16;
					const auto localY = gridY - (source >> 1) * 16;
					if (localX > 16 || localY > 16) {
						continue;
					}
					listedBase = IsListed(a_data->defQuadTextures[source]);
					for (std::size_t layer = 0; layer < 6; ++layer) {
						const int percent = static_cast<std::uint8_t>(a_data->percents[source][localY * 17 + localX][layer]);
						layers += percent;
						listed += IsListed(a_data->quadTextures[source][layer]) ? percent : 0;
					}
				}
				const auto remainder = std::max(0, 255 - layers);
				const auto coverage = listed + (listedBase ? remainder : 0);
				if (coverage == 0) {
					continue;
				}
				const auto total = layers + remainder;
				for (auto& channel : color) {
					const auto original = static_cast<std::uint8_t>(channel);
					const auto normalized = original * 255 / brightest;
					channel = static_cast<std::int8_t>(original + (normalized - original) * coverage / total);
				}
			}
		}
	}

	bool IsInSolstheim(const RE::TESObjectLAND* a_land)
	{
		const auto worldspace = a_land->parentCell ? a_land->parentCell->GetRuntimeData().worldSpace : nullptr;
		return worldspace && _stricmp(worldspace->GetFormEditorID(), "DLC2SolstheimWorld") == 0;
	}

	void RecolorLoadedData(RE::TESObjectLAND* a_land)
	{
		if (skipSolstheim && IsInSolstheim(a_land)) {
			return;
		}
		if (a_land->loadedData && a_land->data.flags.all(RE::OBJ_LAND::Flag::kVertexColors)) {
			Recolor(a_land->loadedData);
		}
	}

	struct LoadLandDataFromFile
	{
		static bool thunk(RE::TESObjectLAND* a_land)
		{
			const auto result = func(a_land);
			if (result) {
				RecolorLoadedData(a_land);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct Load
	{
		static bool thunk(RE::TESObjectLAND* a_land, RE::TESFile* a_file)
		{
			const auto result = func(a_land, a_file);
			if (result && (a_land->data.flags.underlying() & (1 << 3)) != 0) {
				RecolorLoadedData(a_land);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct SetFormEditorID
	{
		static bool thunk(RE::TESLandTexture* a_texture, const char* a_editorID)
		{
			for (const auto& name : listedNames) {
				if (a_editorID && _stricmp(name.c_str(), a_editorID) == 0 && !std::ranges::contains(listedTextures, a_texture)) {
					listedTextures.push_back(a_texture);
					logger::info("Listed landscape texture {}", a_editorID);
				}
			}
			return func(a_texture, a_editorID);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	std::uintptr_t FindCall(std::uintptr_t a_caller, std::uintptr_t a_target)
	{
		for (auto at = a_caller; at < a_caller + 0x200; ++at) {
			if (*reinterpret_cast<const std::uint8_t*>(at) == 0xE8 && at + 5 + *reinterpret_cast<const std::int32_t*>(at + 1) == a_target) {
				return at;
			}
		}
		return 0;
	}

	void LoadConfig()
	{
		const auto config = nlohmann::json::parse(std::ifstream{ "Data/SKSE/Plugins/VertexRemover.json" }, nullptr, false, true);
		if (!config.is_object()) {
			return;
		}
		if (config.contains("SkipSolstheim") && config["SkipSolstheim"].is_boolean()) {
			skipSolstheim = config["SkipSolstheim"].get<bool>();
		}
		if (config.contains("KeepDarkColors") && config["KeepDarkColors"].is_boolean()) {
			keepDarkColors = config["KeepDarkColors"].get<bool>();
		}
		if (config.contains("KeepDarkerThan") && config["KeepDarkerThan"].is_number()) {
			keepDarkerThan = static_cast<int>(std::clamp(config["KeepDarkerThan"].get<double>(), 0.0, 255.0));
		}
		if (!config.contains("OnlyListedTextures") || !config["OnlyListedTextures"].is_boolean() || !config["OnlyListedTextures"].get<bool>()) {
			return;
		}
		onlyListed = true;
		for (const auto& name : config.value("Textures", nlohmann::json::array())) {
			if (name.is_string()) {
				listedNames.push_back(name.get<std::string>());
			}
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse, { .trampoline = true, .trampolineSize = 14 });
	const auto caller = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(18331, 18747) }.address();
	const auto target = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(18332, 18748) }.address();
	const auto site = FindCall(caller, target);
	logger::info("TESObjectLAND load-from-file call site {:X}", site);
	if (!site) {
		return true;
	}
	LoadConfig();
	if (onlyListed) {
		SetFormEditorID::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE_TESLandTexture[0] }.write_vfunc(0x33, SetFormEditorID::thunk);
	}
	LoadLandDataFromFile::func = SKSE::GetTrampoline().write_call<5>(site, LoadLandDataFromFile::thunk);
	Load::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE_TESObjectLAND[0] }.write_vfunc(0x6, Load::thunk);
	return true;
}
