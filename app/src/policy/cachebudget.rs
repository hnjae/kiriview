// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    extern "Rust" {
        #[cxx_name = "rustDisplayImageCacheByteBudgetForSystemMemory"]
        fn rust_display_image_cache_byte_budget_for_system_memory(
            system_memory_byte_size: i64,
            preferred_byte_budget: i64,
        ) -> i64;

        #[cxx_name = "rustDisplayImageCachePreferredByteBudget"]
        fn rust_display_image_cache_preferred_byte_budget() -> i64;

        #[cxx_name = "rustPredecodeCacheByteBudgetForSystemMemory"]
        fn rust_predecode_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64;

        #[cxx_name = "rustThumbnailCachePreferredByteBudget"]
        fn rust_thumbnail_cache_preferred_byte_budget() -> i64;

        #[cxx_name = "rustThumbnailCacheByteBudgetForSystemMemory"]
        fn rust_thumbnail_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64;
    }
}

const DISPLAY_IMAGE_CACHE_SYSTEM_MEMORY_DIVISOR: i64 = 16;
const DISPLAY_IMAGE_CACHE_PREFERRED_BYTE_BUDGET: i64 = 512 * 1024 * 1024;
const PREDECODE_CACHE_PREFERRED_BYTE_BUDGET: i64 = 1024 * 1024 * 1024;
const PREDECODE_CACHE_SYSTEM_MEMORY_DIVISOR: i64 = 8;
const THUMBNAIL_CACHE_PREFERRED_BYTE_BUDGET: i64 = 64 * 1024 * 1024;
const THUMBNAIL_CACHE_SYSTEM_MEMORY_DIVISOR: i64 = 64;

pub(crate) fn system_memory_capped_byte_budget(
    preferred_byte_budget: i64,
    system_memory_byte_size: i64,
    memory_divisor: i64,
) -> i64 {
    if preferred_byte_budget <= 0 {
        return 0;
    }
    if system_memory_byte_size <= 0 || memory_divisor <= 0 {
        return preferred_byte_budget;
    }

    preferred_byte_budget.min(system_memory_byte_size / memory_divisor)
}

fn rust_display_image_cache_byte_budget_for_system_memory(
    system_memory_byte_size: i64,
    preferred_byte_budget: i64,
) -> i64 {
    display_image_cache_byte_budget_for_system_memory(
        system_memory_byte_size,
        preferred_byte_budget,
    )
}

fn rust_display_image_cache_preferred_byte_budget() -> i64 {
    display_image_cache_preferred_byte_budget()
}

fn rust_predecode_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64 {
    predecode_cache_byte_budget_for_system_memory(system_memory_byte_size)
}

fn rust_thumbnail_cache_preferred_byte_budget() -> i64 {
    thumbnail_cache_preferred_byte_budget()
}

fn rust_thumbnail_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64 {
    thumbnail_cache_byte_budget_for_system_memory(system_memory_byte_size)
}

pub(crate) fn display_image_cache_byte_budget_for_system_memory(
    system_memory_byte_size: i64,
    preferred_byte_budget: i64,
) -> i64 {
    system_memory_capped_byte_budget(
        preferred_byte_budget,
        system_memory_byte_size,
        DISPLAY_IMAGE_CACHE_SYSTEM_MEMORY_DIVISOR,
    )
}

pub(crate) fn display_image_cache_preferred_byte_budget() -> i64 {
    DISPLAY_IMAGE_CACHE_PREFERRED_BYTE_BUDGET
}

pub(crate) fn predecode_cache_preferred_byte_budget() -> i64 {
    PREDECODE_CACHE_PREFERRED_BYTE_BUDGET
}

pub(crate) fn predecode_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64 {
    system_memory_capped_byte_budget(
        predecode_cache_preferred_byte_budget(),
        system_memory_byte_size,
        PREDECODE_CACHE_SYSTEM_MEMORY_DIVISOR,
    )
}

pub(crate) fn thumbnail_cache_preferred_byte_budget() -> i64 {
    THUMBNAIL_CACHE_PREFERRED_BYTE_BUDGET
}

pub(crate) fn thumbnail_cache_byte_budget_for_system_memory(system_memory_byte_size: i64) -> i64 {
    system_memory_capped_byte_budget(
        thumbnail_cache_preferred_byte_budget(),
        system_memory_byte_size,
        THUMBNAIL_CACHE_SYSTEM_MEMORY_DIVISOR,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_image_cache_byte_budget_uses_preferred_limit_and_system_memory_cap() {
        let preferred = display_image_cache_preferred_byte_budget();

        assert_eq!(preferred, 512 * 1024 * 1024);
        assert_eq!(
            display_image_cache_byte_budget_for_system_memory(0, preferred),
            preferred
        );
        assert_eq!(
            display_image_cache_byte_budget_for_system_memory(preferred, preferred),
            preferred / DISPLAY_IMAGE_CACHE_SYSTEM_MEMORY_DIVISOR
        );
        assert_eq!(
            display_image_cache_byte_budget_for_system_memory(preferred * 32, preferred),
            preferred
        );
        assert_eq!(
            display_image_cache_byte_budget_for_system_memory(preferred, -1),
            0
        );
    }

    #[test]
    fn predecode_cache_byte_budget_uses_preferred_limit_and_system_memory_cap() {
        let preferred = predecode_cache_preferred_byte_budget();

        assert_eq!(predecode_cache_byte_budget_for_system_memory(0), preferred);
        assert_eq!(
            predecode_cache_byte_budget_for_system_memory(preferred),
            preferred / PREDECODE_CACHE_SYSTEM_MEMORY_DIVISOR
        );
        assert_eq!(
            predecode_cache_byte_budget_for_system_memory(preferred * 16),
            preferred
        );
    }

    #[test]
    fn thumbnail_cache_byte_budget_uses_preferred_limit_and_system_memory_cap() {
        let preferred = thumbnail_cache_preferred_byte_budget();

        assert_eq!(thumbnail_cache_byte_budget_for_system_memory(0), preferred);
        assert_eq!(
            thumbnail_cache_byte_budget_for_system_memory(preferred),
            preferred / THUMBNAIL_CACHE_SYSTEM_MEMORY_DIVISOR
        );
        assert_eq!(
            thumbnail_cache_byte_budget_for_system_memory(preferred * 128),
            preferred
        );
    }
}
