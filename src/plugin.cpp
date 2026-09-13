namespace
{
	void Recolor(RE::TESObjectLAND::LoadedLandData* a_data)
	{
		static const auto curves = std::views::iota(0, 512) | std::views::transform([](int a_index) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(std::pow((a_index % 256) / 255.0, a_index < 256 ? 0.1 : 0.5) * 255.0)); }) | std::ranges::to<std::vector>();
		const auto isSnow = [](const RE::TESLandTexture* a_texture) { return a_texture && a_texture->shaderTextureIndex == 1; };
		const auto cell = [](std::size_t a_quad, std::size_t a_vertex) { return ((a_quad >> 1) * 16 + a_vertex / 17) * 33 + (a_quad & 1) * 16 + a_vertex % 17; };
		std::array<std::array<std::uint16_t, 3>, 1089> blend{};
		for (std::size_t quad = 0; quad < 4; ++quad) {
			for (std::size_t vertex = 0; vertex < 289; ++vertex) {
				blend[cell(quad, vertex)][2] = isSnow(a_data->defQuadTextures[quad]);
				for (std::size_t layer = 0; layer < 6; ++layer) blend[cell(quad, vertex)][isSnow(a_data->quadTextures[quad][layer]) ? 0 : 1] += static_cast<std::uint8_t>(a_data->percents[quad][vertex][layer]);
			}
		}
		for (std::size_t quad = 0; quad < 4; ++quad) {
			for (std::size_t vertex = 0; vertex < 289; ++vertex) {
				const auto [snow, other, snowBase] = blend[cell(quad, vertex)];
				const auto curve = snow > 51 || (snowBase && other < 204) ? 0 : 256;
				for (auto& channel : a_data->colors[quad][vertex]) channel = curves[curve + static_cast<std::uint8_t>(channel)];
			}
		}
	}

	template <class... Args>
	struct LoadHook
	{
		static bool thunk(RE::TESObjectLAND* a_land, Args... a_args)
		{
			const auto result = func(a_land, a_args...);
			if (result && a_land->loadedData && a_land->data.flags.all(RE::OBJ_LAND::Flag::kVertexColors) && (sizeof...(Args) == 0 || a_land->data.flags.underlying() & 8)) Recolor(a_land->loadedData);
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	const auto caller = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(18331, 18747) }.address();
	const auto window = std::views::iota(caller, caller + 0x200);
	const auto site = std::ranges::find_if(window, [target = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(18332, 18748) }.address()](std::uintptr_t a_at) { return *reinterpret_cast<const std::uint8_t*>(a_at) == 0xE8 && a_at + 5 + *reinterpret_cast<const std::int32_t*>(a_at + 1) == target; });
	logger::info("TESObjectLAND load-from-file call site {:X}", site == window.end() ? 0 : *site);
	if (site == window.end()) return true;
	SKSE::GetTrampoline().create(14);
	LoadHook<>::func = SKSE::GetTrampoline().write_call<5>(*site, LoadHook<>::thunk);
	LoadHook<RE::TESFile*>::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE_TESObjectLAND[0] }.write_vfunc(0x6, LoadHook<RE::TESFile*>::thunk);
	return true;
}
