/**
 * @file    goliath_engine/common/logging.h
 * @brief   EOT_* logging macros bound to the eot log category.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 */
#pragma once
#include <rex/logging.h>

REXLOG_DEFINE_CATEGORY(eot)

#define EOT_TRACE(...)    REXLOG_CAT_TRACE(::rex::log::eot(), __VA_ARGS__)
#define EOT_DEBUG(...)    REXLOG_CAT_DEBUG(::rex::log::eot(), __VA_ARGS__)
#define EOT_INFO(...)     REXLOG_CAT_INFO(::rex::log::eot(), __VA_ARGS__)
#define EOT_WARN(...)     REXLOG_CAT_WARN(::rex::log::eot(), __VA_ARGS__)
#define EOT_ERROR(...)    REXLOG_CAT_ERROR(::rex::log::eot(), __VA_ARGS__)
#define EOT_CRITICAL(...) REXLOG_CAT_CRITICAL(::rex::log::eot(), __VA_ARGS__)
