#include "HotbarComponent.h"
#include "Scene.h"
#include "engine.h"
#include "ArchiveUnpacker.h"
#include "components.h"

namespace {
    constexpr float kSlotSpacing = 56.0f;
    constexpr float kSlotSize    = 48.0f;
    constexpr float kIconSize    = 36.0f;
}

void HotbarComponent::Init()
{
    struct Entry { BlockType type; const char* texture; };
    const std::vector<Entry> entries = {
        {BlockType::Dirt,  "block_textures/dirt.png"},
        {BlockType::Stone, "block_textures/stone.png"},
        {BlockType::Grass, "block_textures/grass.png"},
        {BlockType::Sand,  "block_textures/sand.png"},
    };

    auto* archive = Engine::GetResourcesArchive();
    const Vector2 base = object->GetPosition();
    const float centerOffset = (entries.size() - 1) * 0.5f;

    // Sprites render from top-left, so positions below are computed so that
    // each slot's center sits on slotCenter and the icon is centered within it.
    const Vector2 slotHalf(kSlotSize * 0.5f, kSlotSize * 0.5f);
    const Vector2 iconHalf(kIconSize * 0.5f, kIconSize * 0.5f);

    for (size_t i = 0; i < entries.size(); ++i) {
        Vector2 slotCenter = base + Vector2(((float)i - centerOffset) * kSlotSpacing, 0);

        Object* slot = CreateObject();
        slot->SetLayer(900);
        slot->SetPosition(slotCenter - slotHalf);
        slot->SetSize(Vector2(kSlotSize, kSlotSize));
        slot->AddComponent(new Image(archive->GetFile("hotbar_slot.png"),
                                     Vector2(kSlotSize, kSlotSize)));
        hotbarSlots.push_back(slot);

        Object* icon = CreateObject();
        icon->SetLayer(901);
        icon->SetPosition(slotCenter - iconHalf);
        icon->SetSize(Vector2(kIconSize, kIconSize));
        icon->AddComponent(new Image(archive->GetFile(entries[i].texture),
                                     Vector2(kIconSize, kIconSize)));
        hotbarIcons.push_back(icon);

        slotTypes.push_back(entries[i].type);
    }

    selectedSlotIndex = 0;
    selectedSlot = slotTypes[selectedSlotIndex];
    hotbarSlots[selectedSlotIndex]->GetComponent<Image>()->SetNewSprite(
        archive->GetFile("hotbar_slot_selected.png"));
}

void HotbarComponent::SetSelectedSlot(int slot)
{
    if (slot < 0 || slot >= (int)slotTypes.size()) return;

    auto* archive = Engine::GetResourcesArchive();
    for (size_t i = 0; i < hotbarSlots.size(); ++i) {
        hotbarSlots[i]->GetComponent<Image>()->SetNewSprite(
            archive->GetFile("hotbar_slot.png"));
    }

    selectedSlotIndex = slot;
    selectedSlot = slotTypes[slot];
    hotbarSlots[selectedSlotIndex]->GetComponent<Image>()->SetNewSprite(
        archive->GetFile("hotbar_slot_selected.png"));
}

void HotbarComponent::Update()
{
}

BlockType HotbarComponent::GetSelectedSlot() const
{
    return selectedSlot;
}
