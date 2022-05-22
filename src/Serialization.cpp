#include "Serialization.h"

namespace Serialization
{
	void AutoToggleMap::Add(const RE::Actor* a_actor, Biped a_slot, Slot::State a_toggleState, bool a_firstPerson)
	{
		Locker locker(_lock);
		if (a_firstPerson) {
			_map[a_actor->GetFormID()][a_slot].firstPerson = a_toggleState;
		} else {
			_map[a_actor->GetFormID()][a_slot].thirdPerson = a_toggleState;
		}
	}

	bool AutoToggleMap::Remove(const RE::Actor* a_actor)
	{
		Locker locker(_lock);
		return _map.erase(a_actor->GetFormID()) != 0;
	}

	std::optional<Slot::State> AutoToggleMap::GetToggleState(const RE::Actor* a_actor, Biped a_slot, bool a_firstPerson)
	{
		Locker locker(_lock);
		if (auto ptrIt = _map.find(a_actor->GetFormID()); ptrIt != _map.end()) {
			if (auto slotIt = ptrIt->second.find(a_slot); slotIt != ptrIt->second.end()) {
				return a_firstPerson ? slotIt->second.firstPerson : slotIt->second.thirdPerson;
			}
		}
		return std::nullopt;
	}

	void AutoToggleMap::Clear()
	{
		Locker locker(_lock);
		_map.clear();
	}

	bool AutoToggleMap::Save(SKSE::SerializationInterface* a_intfc, std::uint32_t a_type, std::uint32_t a_version)
	{
		if (!a_intfc->OpenRecord(a_type, a_version)) {
			logger::error("Failed to open serialization record!"sv);
			return false;
		} else {
			return Save(a_intfc);
		}
	}

	bool AutoToggleMap::Save(SKSE::SerializationInterface* a_intfc)
	{
		assert(a_intfc);
		Locker locker(_lock);

		const std::size_t numActors = _map.size();
		if (!a_intfc->WriteRecordData(numActors)) {
			logger::error("Failed to save number of actors ({})", numActors);
			return false;
		}

		for (const auto& [formID, slots] : _map) {
			if (!a_intfc->WriteRecordData(formID)) {
				logger::error("Failed to save formID ({:X})", formID);
				return false;
			}
			const std::size_t numSlots = slots.size();
			if (!a_intfc->WriteRecordData(numSlots)) {
				logger::error("Failed to save number of toggles ({})", numActors);
				return false;
			}
			for (const auto& [slot, state] : slots) {
				if (!a_intfc->WriteRecordData(slot)) {
					logger::error("Failed to save slot ({}) for {:X}", slot, formID);
					return false;
				}
				auto fP = state.firstPerson ? stl::to_underlying(*state.firstPerson) : -1;
				if (!a_intfc->WriteRecordData(fP)) {
					logger::error("Failed to save first person toggle state ({}) for {:X}", fP, formID);
					return false;
				}
				auto tP = state.thirdPerson ? stl::to_underlying(*state.thirdPerson) : -1;
				if (!a_intfc->WriteRecordData(tP)) {
					logger::error("Failed to save third person toggle state ({}) for {:X}", tP, formID);
					return false;
				}
			}
		}

		return true;
	}

	bool AutoToggleMap::Load(SKSE::SerializationInterface* a_intfc)
	{
		assert(a_intfc);
		std::size_t size;
		a_intfc->ReadRecordData(size);

		Locker locker(_lock);
		_map.clear();

		RE::FormID formID;

		std::size_t numSlots;
		Biped slot;

		for (std::size_t i = 0; i < size; i++) {
			a_intfc->ReadRecordData(formID);
			if (!a_intfc->ResolveFormID(formID, formID)) {
				logger::error("Failed to resolve formID {}"sv, formID);
				continue;
			}
			a_intfc->ReadRecordData(numSlots);
			for (std::size_t j = 0; j < numSlots; j++) {
				a_intfc->ReadRecordData(slot);

				std::int32_t fP = -1;
				std::int32_t tP = -1;
			    ToggleState state{ std::nullopt, std::nullopt };

				a_intfc->ReadRecordData(fP);
				if (fP != -1) {
					state.firstPerson = static_cast<Slot::State>(fP);
				}
				a_intfc->ReadRecordData(tP);
				if (tP != -1) {
					state.thirdPerson = static_cast<Slot::State>(tP);
				}

				_map[formID].insert_or_assign(slot, state);
			}
		}

		logger::info("Loaded {} entries", _map.size());

		return true;
	}

	std::string DecodeTypeCode(std::uint32_t a_typeCode)
	{
		constexpr std::size_t SIZE = sizeof(std::uint32_t);

		std::string sig;
		sig.resize(SIZE);
		char* iter = reinterpret_cast<char*>(&a_typeCode);
		for (std::size_t i = 0, j = SIZE - 2; i < SIZE - 1; ++i, --j) {
			sig[j] = iter[i];
		}
		return sig;
	}

	void SaveCallback(SKSE::SerializationInterface* a_intfc)
	{
		const auto map = AutoToggleMap::GetSingleton();
		if (!map->Save(a_intfc, kAutoToggle, kSerializationVersion)) {
			logger::critical("Failed to save auto toggle regs!"sv);
		}

		logger::info("Finished saving data"sv);
	}

	void LoadCallback(SKSE::SerializationInterface* a_intfc)
	{
		std::uint32_t type;
		std::uint32_t version;
		std::uint32_t length;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (version != kSerializationVersion) {
				logger::critical("Loaded data is out of date! Read ({}), expected ({}) for type code ({})", version, kSerializationVersion, DecodeTypeCode(type));
				continue;
			}
			if (type == kAutoToggle) {
				const auto map = AutoToggleMap::GetSingleton();
				if (!map->Load(a_intfc)) {
					logger::critical("Failed to load auto toggle regs!"sv);
				}
			} else {
				logger::critical("Unrecognized record type ({})!", DecodeTypeCode(type));
			}
		}
	}

	void RevertCallback(SKSE::SerializationInterface*)
	{
		AutoToggleMap::GetSingleton()->Clear();

		logger::info("Reverting...");
	}

	void SetToggleState(const RE::Actor* a_actor, const Biped a_slot, Slot::State a_state, bool a_firstPerson)
	{
		Biped slot;
		if (headSlots.find(a_slot) != headSlots.end()) {
			slot = Biped::kHead;
		} else {
			slot = a_slot;
		}

		AutoToggleMap::GetSingleton()->Add(a_actor, slot, a_state, a_firstPerson);
	}

	Slot::State GetToggleState(const RE::Actor* a_actor, const Biped a_slot, bool a_firstPerson)
	{
		Biped slot;
		if (headSlots.find(a_slot) != headSlots.end()) {
			slot = Biped::kHead;
		} else {
			slot = a_slot;
		}

		if (const auto state = AutoToggleMap::GetSingleton()->GetToggleState(a_actor, slot, a_firstPerson); state) {
			return *state;
		} else {
			AutoToggleMap::GetSingleton()->Add(a_actor, slot, Slot::State::kHide, a_firstPerson);
			return Slot::State::kHide;
		}
	}
}
