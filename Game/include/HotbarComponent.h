#ifndef HotbarComponent_H
#define HotbarComponent_H

#include "component.h"
#include "BlockComponent.h"

#include <vector>

class HotbarComponent : public Component
{
public:
    void Init() override;
    void Update() override;

    BlockType GetSelectedSlot() const;
    void SetSelectedSlot(int slot);
    int GetSlotCount() const { return (int)slotTypes.size(); }

private:
    BlockType selectedSlot = BlockType::Grass;
    int selectedSlotIndex = 0;

    std::vector<Object*> hotbarSlots;
    std::vector<Object*> hotbarIcons;
    std::vector<BlockType> slotTypes;
};

#endif // HotbarComponent_H
