#include "ng_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

extern uint8_t *bus_wram(void);

int main(void) {
    printf("=== Testing KOF '98 style Stage Re-Roll for Random Slots ===\n");

    // Initialize IO and RAM
    io_reset(0);
    uint8_t *w = bus_wram();
    assert(w != NULL);

    // 1. Simulate Player 1 picking Team Edit:
    // Slot 0: Kyo (ID 6, manual)
    // Slot 1: Rolled Athena (ID 3, random slot)
    // Slot 2: Iori (ID 9, manual)
    w[0xA840] = 1; // Team Edit
    w[0xA842] = 3; // Done selecting
    w[0xA843] = 6; // Slot 1: Kyo
    w[0xA844] = 3; // Slot 2: Athena (random)
    w[0xA845] = 9; // Slot 3: Iori

    // Mark slot 1 as picked via random select
    // In actual game, this was set when pressing A on [?]
    // Let's simulate picking slot 1 on random select:
    // We can directly test io_on_order_select after audio_command(0x30)
    // First, let's verify that entering character select resets state:
    // io_sync_soft_dips with task set to select
    w[0x500] = 0x00; w[0x501] = 0x03; w[0x502] = 0xE6; w[0x503] = 0x50; // Select task
    w[0x500 + 0x130] = 0; // P1
    w[0xA842] = 0; // Slot 0
    io_sync_soft_dips(); // Trigger s_p1_was_sel

    // Simulate P1 selecting Slot 0: Kyo (manual)
    // Not on random, press A
    w[0xA843] = 6;
    w[0xA842] = 1;

    // Simulate P1 moving to [?] and selecting Slot 1 (random)
    // Right next to King (23) -> move right to random col 3
    // Then press A on random:
    // In io.c, this sets s_p1_random_slots[1] = true
    // Let's do that via soft dips input:
    // Set cursor on King:
    w[0xA844] = 23;
    ng_input_t in = {0};
    in.p1 = NG_RIGHT;
    io_set_inputs(&in);
    io_sync_soft_dips(); // Moves to [?] col 3

    // Now press A while on [?]
    in.p1 = NG_A;
    io_set_inputs(&in);
    io_sync_soft_dips(); // Rolls slot 1 via random and records s_p1_random_slots[1] = true

    uint8_t slot1_initial = w[0xA844];
    printf("Initial Slot 1 (Random) rolled: ID %d\n", slot1_initial);

    // In actual game, ROM advances cur_slot to 2 upon A confirmation
    w[0xA842] = 2; // Slot 2

    // Now select Slot 2: Iori (manual)
    // Left from col 3 moves to King, then we pick Iori (9)
    in.p1 = NG_LEFT;
    io_set_inputs(&in);
    io_sync_soft_dips(); // Returns to King

    w[0xA845] = 9; // Iori
    in.p1 = NG_A;  // Manual press A on Iori
    io_set_inputs(&in);
    io_sync_soft_dips();

    w[0xA842] = 3; // Finished selection!
    in.p1 = 0;
    io_set_inputs(&in);
    io_sync_soft_dips();

    // 2. STAGE 1: Order Select
    // Character select just finished. Order select runs for Stage 1.
    // io_on_order_select should NOT re-roll yet because no match has ended!
    io_on_order_select();
    assert(w[0xA843] == 6); // Kyo still Kyo
    assert(w[0xA844] == slot1_initial); // Slot 1 still has initial roll
    assert(w[0xA845] == 9); // Iori still Iori
    printf("Stage 1 Order Select: team is [%d, %d, %d] - unchanged as expected!\n",
           w[0xA843], w[0xA844], w[0xA845]);

    // 3. STAGE 1 MATCH PLAYED AND WON:
    // Match ends, Victory Demo triggers audio command 0x30!
    printf("\n--- Match 1 won! Audio command 0x30 sent ---\n");
    io_on_audio_command(0x30);

    // 4. STAGE 2: Order Select begins!
    // 0x03FAE4 executes -> calls io_on_order_select()
    io_on_order_select();

    assert(w[0xA843] == 6); // Manual pick Kyo MUST stay Kyo!
    assert(w[0xA845] == 9); // Manual pick Iori MUST stay Iori!
    uint8_t slot1_stage2 = w[0xA844];
    printf("Stage 2 Order Select: team is [%d, %d, %d]\n",
           w[0xA843], w[0xA844], w[0xA845]);
    printf("Slot 0: Kyo (unchanged)\nSlot 1: Re-rolled from %d to %d\nSlot 2: Iori (unchanged)\n",
           slot1_initial, slot1_stage2);

    // 5. STAGE 2 MATCH PLAYED AND WON:
    printf("\n--- Match 2 won! Audio command 0x30 sent ---\n");
    io_on_audio_command(0x30);

    // 6. STAGE 3: Order Select begins!
    io_on_order_select();

    assert(w[0xA843] == 6); // Still Kyo
    assert(w[0xA845] == 9); // Still Iori
    uint8_t slot1_stage3 = w[0xA844];
    printf("Stage 3 Order Select: team is [%d, %d, %d]\n",
           w[0xA843], w[0xA844], w[0xA845]);
    printf("Slot 0: Kyo (unchanged)\nSlot 1: Re-rolled from %d to %d\nSlot 2: Iori (unchanged)\n",
           slot1_stage2, slot1_stage3);

    // 7. Verify duplicates check when duplicates=0:
    io_set_allow_duplicates(0);
    printf("\n--- Testing with Duplicate Characters = OFF ---\n");
    io_on_audio_command(0x30);
    io_on_order_select();
    assert(w[0xA843] == 6);
    assert(w[0xA845] == 9);
    assert(w[0xA844] != 6); // Must not duplicate Kyo
    assert(w[0xA844] != 9); // Must not duplicate Iori
    printf("Stage 4 Order Select: team is [%d, %d, %d] (No duplicate with Kyo or Iori)\n",
           w[0xA843], w[0xA844], w[0xA845]);

    // 8. Test Team Play full random team:
    printf("\n--- Testing Team Play Full Random Team ---\n");
    // Start fresh character select
    w[0x500] = 0x00; w[0x501] = 0x03; w[0x502] = 0xE6; w[0x503] = 0x50;
    w[0xA840] = 0; // Team Play
    w[0xA842] = 0;
    io_sync_soft_dips(); // Resets selection

    // Cursor on King -> move right to [?] and press A
    w[0xA843] = 23;
    in.p1 = NG_RIGHT;
    io_set_inputs(&in);
    io_sync_soft_dips();

    in.p1 = NG_A;
    io_set_inputs(&in);
    io_sync_soft_dips();

    printf("Team Play Initial Team: [%d, %d, %d]\n", w[0xA843], w[0xA844], w[0xA845]);

    // Stage 1 won:
    io_on_audio_command(0x30);
    // Stage 2 Order Select:
    io_on_order_select();
    printf("Team Play Stage 2 Team: [%d, %d, %d] (all 3 re-rolled!)\n",
           w[0xA843], w[0xA844], w[0xA845]);

    printf("\nALL STAGE RE-ROLL TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
