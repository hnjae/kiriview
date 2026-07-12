// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use super::RustPredecodeCachedImageState;

#[derive(Clone, Copy)]
struct RetainedCachedImage {
    original_index: usize,
    current_displayed: bool,
    recent_displayed: bool,
    current_displayed_priority: usize,
    recent_displayed_priority: usize,
    window_priority: usize,
    byte_cost: i64,
    last_used_sequence: u64,
}

pub(super) fn retained_cached_image_indices(
    states: Vec<RustPredecodeCachedImageState>,
    window_count: usize,
    byte_budget: i64,
) -> Vec<usize> {
    if byte_budget <= 0 {
        return Vec::new();
    }

    let mut images: Vec<RetainedCachedImage> = states
        .into_iter()
        .enumerate()
        .filter_map(|(original_index, state)| {
            if state.byte_cost <= 0 {
                return None;
            }

            Some(RetainedCachedImage {
                original_index,
                current_displayed: state.current_displayed,
                recent_displayed: state.recent_displayed,
                current_displayed_priority: state.current_displayed_priority,
                recent_displayed_priority: state.recent_displayed_priority,
                window_priority: state.window_priority,
                byte_cost: state.byte_cost,
                last_used_sequence: state.last_used_sequence,
            })
        })
        .collect();

    let mut current_displayed_images: Vec<RetainedCachedImage> = images
        .iter()
        .copied()
        .filter(|image| image.current_displayed)
        .collect();
    current_displayed_images.sort_by(|left, right| {
        left.current_displayed_priority
            .cmp(&right.current_displayed_priority)
            .then(left.original_index.cmp(&right.original_index))
    });

    let current_displayed_byte_cost = current_displayed_images
        .iter()
        .fold(0_i64, |total, image| total.saturating_add(image.byte_cost));
    let adjacent_byte_budget = byte_budget.saturating_sub(current_displayed_byte_cost);

    images.retain(|image| !image.current_displayed);
    images.sort_by(|left, right| {
        retention_group(*left, window_count)
            .cmp(&retention_group(*right, window_count))
            .then_with(|| retention_priority(*left, *right, window_count))
            .then(left.original_index.cmp(&right.original_index))
    });

    let mut retained_images = Vec::new();
    let mut retained_byte_cost = 0_i64;
    for image in images {
        if retained_byte_cost.saturating_add(image.byte_cost) > adjacent_byte_budget {
            break;
        }

        retained_byte_cost = retained_byte_cost.saturating_add(image.byte_cost);
        retained_images.push(image);
    }

    current_displayed_images.extend(retained_images);
    current_displayed_images
        .into_iter()
        .map(|image| image.original_index)
        .collect()
}

fn retention_group(image: RetainedCachedImage, window_count: usize) -> usize {
    if image.recent_displayed {
        0
    } else if image.window_priority < window_count {
        1
    } else {
        2
    }
}

fn retention_priority(
    left: RetainedCachedImage,
    right: RetainedCachedImage,
    window_count: usize,
) -> std::cmp::Ordering {
    if left.recent_displayed && right.recent_displayed {
        left.recent_displayed_priority
            .cmp(&right.recent_displayed_priority)
    } else if left.window_priority < window_count && right.window_priority < window_count {
        left.window_priority.cmp(&right.window_priority)
    } else {
        right.last_used_sequence.cmp(&left.last_used_sequence)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn retained_cached_image_indices_keep_warm_images_after_window_images() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    cached_image_state(2, 10),
                    cached_image_state(3, 10),
                    cached_image_state(0, 10),
                    cached_image_state(1, 10),
                ],
                3,
                100,
            ),
            vec![2, 3, 0, 1]
        );
    }

    #[test]
    fn retained_cached_image_indices_trim_lowest_priority_images_to_budget() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    cached_image_state(2, 50),
                    cached_image_state(0, 60),
                    cached_image_state(1, 60),
                ],
                3,
                120,
            ),
            vec![1, 2]
        );
    }

    #[test]
    fn retained_cached_image_indices_preserve_priority_prefix_when_trimming() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    cached_image_state(0, 100),
                    cached_image_state(1, 100),
                    cached_image_state(2, 1),
                ],
                3,
                101,
            ),
            vec![0]
        );
    }

    #[test]
    fn retained_cached_image_indices_keep_displayed_images_before_adjacent_budget() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    cached_image_state(0, 60),
                    displayed_cached_image_state(1, 90),
                    cached_image_state(2, 60),
                    displayed_cached_image_state(3, 90),
                ],
                4,
                160,
            ),
            vec![1, 3]
        );
    }

    #[test]
    fn retained_cached_image_indices_keep_displayed_images_without_window() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    displayed_cached_image_state(usize::MAX, 90),
                    cached_image_state(0, 10),
                ],
                0,
                80,
            ),
            vec![0]
        );
    }

    #[test]
    fn retained_cached_image_indices_prioritize_current_recent_then_window() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    cached_image_state(0, 80),
                    recent_cached_image_state(0, 80),
                    displayed_cached_image_state(0, 80),
                ],
                1,
                160,
            ),
            vec![2, 1]
        );
    }

    #[test]
    fn retained_cached_image_indices_evict_oldest_warm_image_first() {
        assert_eq!(
            retained_cached_image_indices(
                vec![
                    warm_cached_image_state(3, 10),
                    cached_image_state(0, 10),
                    warm_cached_image_state(7, 10),
                ],
                1,
                20,
            ),
            vec![1, 2]
        );
    }

    fn cached_image_state(window_priority: usize, byte_cost: i64) -> RustPredecodeCachedImageState {
        RustPredecodeCachedImageState {
            current_displayed: false,
            recent_displayed: false,
            current_displayed_priority: usize::MAX,
            recent_displayed_priority: usize::MAX,
            window_priority,
            byte_cost,
            last_used_sequence: 0,
        }
    }

    fn displayed_cached_image_state(
        window_priority: usize,
        byte_cost: i64,
    ) -> RustPredecodeCachedImageState {
        RustPredecodeCachedImageState {
            current_displayed: true,
            recent_displayed: false,
            current_displayed_priority: window_priority,
            recent_displayed_priority: usize::MAX,
            window_priority,
            byte_cost,
            last_used_sequence: 0,
        }
    }

    fn recent_cached_image_state(
        recent_displayed_priority: usize,
        byte_cost: i64,
    ) -> RustPredecodeCachedImageState {
        RustPredecodeCachedImageState {
            current_displayed: false,
            recent_displayed: true,
            current_displayed_priority: usize::MAX,
            recent_displayed_priority,
            window_priority: usize::MAX,
            byte_cost,
            last_used_sequence: 0,
        }
    }

    fn warm_cached_image_state(
        last_used_sequence: u64,
        byte_cost: i64,
    ) -> RustPredecodeCachedImageState {
        RustPredecodeCachedImageState {
            current_displayed: false,
            recent_displayed: false,
            current_displayed_priority: usize::MAX,
            recent_displayed_priority: usize::MAX,
            window_priority: usize::MAX,
            byte_cost,
            last_used_sequence,
        }
    }
}
