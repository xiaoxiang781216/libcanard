/*
 * Copyright (c) 2016-2020 UAVCAN Development Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <catch.hpp>
#include <canard.h>

// Independent implementation of ID layout from the specification.
#define CONSTRUCT_PRIO(prio) ((std::uint8_t(prio) & 0x7U) << 26U)
#define CONSTRUCT_SUBJECT_ID(subject_id) ((std::uint16_t(subject_id) & 0xFFFFU) << 8U)
#define CONSTRUCT_SOURCE_ID(source_id) ((std::uint8_t(source_id) & 0x7FU) << 1U)
#define CONSTRUCT_SERVICE_ID(service_id) ((std::uint16_t(service_id) & 0x1FFU) << 15U)
#define CONSTRUCT_DEST_ID(dest_id) ((std::uint8_t(dest_id) & 0x7FU) << 8U)
#define CONSTRUCT_REQUEST(request) ((std::uint8_t(request) & 0x01U) << 24U)

#define CONSTRUCT_ANON_BIT (1U << 24U)
#define CONSTRUCT_SNM_BIT (1U << 25U)
#define CONSTRUCT_PV_BIT (1U)

#define CONSTRUCT_MSG_ID(prio, subject_id, source_id)                                                              \
    (CONSTRUCT_PRIO(prio) | CONSTRUCT_SUBJECT_ID(subject_id) | CONSTRUCT_SOURCE_ID(source_id) | CONSTRUCT_PV_BIT | \
     CANARD_CAN_FRAME_EFF)

#define CONSTRUCT_ANON_MSG_ID(prio, subject_id, source_id) \
    (CONSTRUCT_ANON_BIT | CONSTRUCT_MSG_ID(prio, subject_id, source_id))

#define CONSTRUCT_SVC_ID(prio, service_id, request, dest_id, source_id)                                         \
    (CONSTRUCT_PRIO(prio) | CONSTRUCT_SNM_BIT | CONSTRUCT_REQUEST(request) | CONSTRUCT_SERVICE_ID(service_id) | \
     CONSTRUCT_DEST_ID(dest_id) | CONSTRUCT_SOURCE_ID(source_id) | CONSTRUCT_PV_BIT | CANARD_CAN_FRAME_EFF)

#define CONSTRUCT_START(start) ((std::uint8_t(start) & 0x1U) << 7U)
#define CONSTRUCT_END(end) ((std::uint8_t(end) & 0x1U) << 6U)
#define CONSTRUCT_TOGGLE(toggle) ((std::uint8_t(toggle) & 0x1U) << 5U)
#define CONSTRUCT_TID(tid) ((std::uint8_t(tid) & 0x1FU))

#define CONSTRUCT_TAIL_BYTE(start, end, toggle, tid) \
    (CONSTRUCT_START(start) | CONSTRUCT_END(end) | CONSTRUCT_TOGGLE(toggle) | CONSTRUCT_TID(tid))

static bool g_should_accept = true;

/**
 * This callback is invoked by the library when a new message or request or response is received.
 */
static void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer)
{
    (void) ins;
    (void) transfer;
}

static bool shouldAcceptTransfer(const CanardInstance* ins,
                                 uint16_t              port_id,
                                 CanardTransferKind    transfer_kind,
                                 uint8_t               source_node_id)
{
    (void) ins;
    (void) port_id;
    (void) transfer_kind;
    (void) source_node_id;
    return g_should_accept;
}

TEST_CASE("canardHandleRxFrame incompatible packet handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    // Setup frame data to be single frame transfer
    frame.data[0] = CONSTRUCT_TAIL_BYTE(1, 1, 1, 0);

    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    g_should_accept = true;

    // Frame with good RTR/ERR/data_len bad EFF
    frame.id       = 0;
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_INCOMPATIBLE_PACKET == err);

    // Frame with good EFF/ERR/data_len, bad RTR
    frame.id       = 0U | CANARD_CAN_FRAME_RTR | CANARD_CAN_FRAME_EFF;
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_INCOMPATIBLE_PACKET == err);

    // Frame with good EFF/RTR/data_len, bad ERR
    frame.id       = 0U | CANARD_CAN_FRAME_ERR | CANARD_CAN_FRAME_EFF;
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_INCOMPATIBLE_PACKET == err);

    // Frame with good EFF/RTR/ERR, bad data_len
    frame.id       = 0U | CANARD_CAN_FRAME_EFF;
    frame.data_len = 0;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_INCOMPATIBLE_PACKET == err);

    // Frame with good EFF/RTR/ERR/data_len
    frame.id       = 0U | CANARD_CAN_FRAME_EFF;
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);
}

TEST_CASE("canardHandleRxFrame wrong address handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    // Setup frame data to be single frame transfer
    frame.data[0] = CONSTRUCT_TAIL_BYTE(1, 1, 1, 0);

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    canardSetLocalNodeID(&canard, 20);
    g_should_accept = true;

    // Send package with ID 24, should not be wanted
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 24, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_WRONG_ADDRESS == err);

    // Send package with ID 20, should be OK
    frame.id       = 0U | CANARD_CAN_FRAME_EFF;  // Set EFF
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);
}

TEST_CASE("canardHandleRxFrame shouldAccept handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    // Setup frame data to be single frame transfer
    frame.data[0] = CONSTRUCT_TAIL_BYTE(1, 1, 1, 0);

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    canardSetLocalNodeID(&canard, 20);

    // Send packet, don't accept
    g_should_accept = false;
    frame.id        = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len  = 1;
    err             = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_NOT_WANTED == err);

    // Send packet, accept
    g_should_accept = true;
    frame.id        = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len  = 1;
    err             = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);
}

TEST_CASE("canardHandleRxFrame no state handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    g_should_accept = true;

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    canardSetLocalNodeID(&canard, 20);

    // Not start or end of packet, should fail
    frame.data[0]  = CONSTRUCT_TAIL_BYTE(0, 0, 1, 0);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_MISSED_START == err);

    // End of packet, should fail
    frame.data[0]  = CONSTRUCT_TAIL_BYTE(0, 1, 1, 0);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_MISSED_START == err);

    // 1 Frame packet, should pass
    frame.data[0]  = CONSTRUCT_TAIL_BYTE(1, 1, 1, 0);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);

    // Send a start packet, should pass
    frame.data[7] = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);

    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);

    // Send a middle packet, from the same ID, but don't toggle
    frame.data[0]  = CONSTRUCT_TAIL_BYTE(0, 0, 1, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 1;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_WRONG_TOGGLE == err);

    // Send a middle packet, toggle, but use wrong ID
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(0, 0, 0, 2);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_UNEXPECTED_TID == err);

    // Send a middle packet, toggle, and use correct ID
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(0, 0, 0, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);
}

TEST_CASE("canardHandleRxFrame missed start handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    g_should_accept = true;

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    canardSetLocalNodeID(&canard, 20);

    // Send a start packet, should pass
    frame.data[7] = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);

    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);

    // Send a middle packet, toggle, and use correct ID - but timeout
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(0, 0, 0, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;
    err            = canardHandleRxFrame(&canard, &frame, 4000000);
    REQUIRE(-CANARD_ERROR_RX_MISSED_START == err);

    // Send a start packet, should pass
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);

    // Send a middle packet, toggle, and use correct ID - but timestamp 0
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(0, 0, 0, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;
    err            = canardHandleRxFrame(&canard, &frame, 0);
    REQUIRE(-CANARD_ERROR_RX_MISSED_START == err);

    // Send a start packet, should pass
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(CANARD_OK == err);

    // Send a middle packet, toggle, and use an incorrect TID
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(0, 0, 0, 3);
    frame.data[7]  = 0U | 3U | (1U << 5U);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;
    err            = canardHandleRxFrame(&canard, &frame, 0);
    REQUIRE(-CANARD_ERROR_RX_MISSED_START == err);
}

TEST_CASE("canardHandleRxFrame short frame handling, Correctness")
{
    uint8_t        canard_memory_pool[1024];
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    g_should_accept = true;

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard,
               canard_memory_pool,
               sizeof(canard_memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               &canard);
    canardSetLocalNodeID(&canard, 20);

    // Send a start packet which is short, should fail
    frame.data[1]  = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 2;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_SHORT_FRAME == err);

    // Send a start packet which is short, should fail
    frame.data[2]  = CONSTRUCT_TAIL_BYTE(1, 0, 1, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 3;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_RX_SHORT_FRAME == err);
}

TEST_CASE("canardHandleRxFrame OOM handling, Correctness")
{
    uint8_t        dummy_buf;
    CanardInstance canard;
    CanardCANFrame frame;
    int16_t        err;

    g_should_accept = true;

    // Open canard to accept all transfers with a node ID of 20
    canardInit(&canard, &dummy_buf, 0, onTransferReceived, shouldAcceptTransfer, &canard);
    canardSetLocalNodeID(&canard, 20);

    // Send a start packet - we shouldn't have any memory
    frame.data[7]  = CONSTRUCT_TAIL_BYTE(1, 0, 0, 1);
    frame.id       = CONSTRUCT_SVC_ID(0, 0, 1, 20, 0);
    frame.data_len = 8;  // Data length MUST be full packet
    err            = canardHandleRxFrame(&canard, &frame, 1);
    REQUIRE(-CANARD_ERROR_OUT_OF_MEMORY == err);
}
