<!--
    Saga4 Endless List component
    NOTE: The (vertical) container must have an absolute height: 100px or 100vh is ok, 100% is not, height:0 with flex-grow:1 is also ok
-->
<template>
    <div ref="outer" class="s4-infinite-list" :style="css_props" @scroll="onscroll" @wheel="onwheel">
        <div class="s4-infinite-list-header">
            <slot name="header">
            </slot>
        </div>
        <template v-for="(item, idx) in items">
            <div :key="start_idx + idx" class="s4-infinite-list-item">
                <slot name="item" :idx="start_idx + idx" :item="item">
                    <span>{{ item }}</span>
                </slot>
            </div>
        </template>
        <div class="s4-infinite-list-footer">
            <slot name="footer">
            </slot>
        </div>
    </div>
</template>

<script>

export default {
    name: "S4InfiniteList",
    props: {
        title: String,
        scrollSpeed: {
            type: Number,
            default: 16,
        },
        guard: {
            type: Number,
            default: 3,
        },
        preload: {
            type: Number,
            default: 8, // Must be >= guard. No way to actually validate this.
        },
        maxItems: {
            type: Number,
            default: 0, // Must be >= 2*guard + preload + what fits on the screen. If it's not, we'll silently override it.
        },
        startIdx: {
            type: Number,
            default: 0,
        },
        horizontal: {
            type: Boolean,
            default: false,
        },
        supplyItems: {
            type: Function,
            default: function (from, to) {
                let new_items = [];
                for (let i = from; i < to; i++) {
                    new_items.push(i);
                }
                return new_items;
            },
        },
    },
    data() {
        return {
            start_idx: this.startIdx,
            items: [],
            max_items: this.maxItems, // We may need to increase it to prevent cache thrashing, see the docs for details.
            reached_first_item: false,
            reached_last_item: false,
            reference_element: null, // An item that keeps existing after a re-rendering: we use it to determine the new scroll position
            displacement_from_reference: 0,
            check_cache_thrashing: false,
            first_rendering: true,
        };
    },
    computed: {
        css_props: function () {
            return {
                "--s4-infinite-list-direction": this.horizontal ? "row" : "column",
            };
        },
    },
    methods: {
        onwheel: function (e) {
            // The "smooth scroll" of Chrome simulates a bunch of onscroll events for each onwheel, but
            // even if the container content and thus its scroll position is changed programmatically meanwhile,
            // the already generated onscroll events set back the old scroll positions.
            // Disable chrome://flags/#smooth-scrolling and you won't need this, but our users still do.
            e.preventDefault();
            e.currentTarget.scrollBy(0, (e.deltaY > 0) ? this.scrollSpeed : -this.scrollSpeed);
        },
        start_of: function (e) {
            if (this.horizontal) {
                return e.offsetLeft;
            }
            else {
                return e.offsetTop;
            }
        },
        size_of: function (e) {
            if (this.horizontal) {
                return e.offsetWidth;
            }
            else {
                return e.offsetHeight;
            }
        },
        onscroll: function (e) {
            // If a re-rendering is due, then the element positions are out of sync, so don't even try to do anything
            if (this.reference_element) {
                return;
            }

            // Cache thrashing prevention: if it would happen after an expansion, then don't evict elements from the other end, but increase array length to fit
            let grow_instead_of_eviction = this.check_cache_thrashing || (this.maxItems == 0);
            this.check_cache_thrashing = false;

            // Displayed positions in container-pixel-domain
            let display_start = this.horizontal ? e.target.scrollLeft : e.target.scrollTop;
            let display_end = display_start + this.size_of(e.target);

            let container_len = this.$refs.outer.children.length;

            // Start guard: container-element-domain: bottom of the Nth element (+1 for header) or the footer, if there are too few of them
            let start_guard_idx = Math.min(this.guard, container_len - 1);
            let start_guard = this.$refs.outer.children.item(start_guard_idx);
            let start_guard_pos = this.start_of(start_guard) + this.size_of(start_guard); // the bottom line of the Nth element
            let is_near_start = display_start <= start_guard_pos;

            // End guard: container-element-domain: top of the Nth last element (-1 for footer) or the header, if there are too few of them
            let end_guard_idx = Math.max(container_len - 1 - this.guard, 0);
            let end_guard = this.$refs.outer.children.item(end_guard_idx);
            let end_guard_pos = this.start_of(end_guard); // the top line of the Nth last element
            let is_near_end = end_guard_pos < display_end;

            // If a guard position is visible, it means we're near to that end and should expand the array in that direction,
            // if we're not yet at the beginning/end of the data

            if (is_near_start && !this.reached_first_item) {
                let new_items = this.supplyItems(this.start_idx - this.preload, this.start_idx, true);
                // The provider may signal us that there won't be any prior items
                if (new_items[0] == null) {
                    this.reached_first_item = true;
                    new_items.shift();
                }
                this.start_idx -= new_items.length; // Keep the ":key"-s consistent and unchanged, so already existing elements are re-used and not re-rendered


                // find an item that'll stay alive and relate the display top scroll position to it
                this.reference_element = this.$refs.outer.children.item(1); // [0] is the header
                this.displacement_from_reference = display_start - this.start_of(this.reference_element);
                this.items.splice(0, 0, ...new_items);

                // if there are too many items, evict some from the other end...
                if (this.max_items < this.items.length) {
                    if (grow_instead_of_eviction) { // ... unless it would cause cache thrashing
                        this.max_items = this.items.length;
                    }
                    else {
                        this.items.splice(this.max_items);
                        this.reached_last_item = false;
                        this.check_cache_thrashing = true;
                    }
                }
            }

            // the same for bottom
            if (is_near_end && !this.reached_last_item) {
                let n = this.start_idx + this.items.length;
                let new_items = this.supplyItems(n, n + this.preload, false);
                if (new_items[new_items.length - 1] == null) {
                    this.reached_last_item = true;
                    new_items.pop();
                }

                this.reference_element = this.$refs.outer.children.item(this.$refs.outer.children.length - 2); // [length-1] is the footer
                this.displacement_from_reference = display_start - this.start_of(this.reference_element);
                this.items.splice(this.items.length, 0, ...new_items);
                n = this.items.length - this.max_items;
                if (0 < n) {
                    if (grow_instead_of_eviction) {
                        this.max_items = this.items.length;
                    }
                    else {
                        this.start_idx += n;
                        this.items.splice(0, n);
                        this.reached_first_item = false;
                        this.check_cache_thrashing = true;
                    }
                }
            }
        },
    },
    updated: function () {
        // if re-rendered due to an expansion, then restore the scroll position related to a surviving (and re-rendered) element
        if (this.first_rendering) {
            this.$refs.outer.children.item(1 + this.startIdx - this.start_idx).scrollIntoView(true);
            this.first_rendering = false;
            this.reference_element = null;
        }
        else if (this.reference_element) {
            let restore_pos = this.start_of(this.reference_element) + this.displacement_from_reference;
            if (this.horizontal) {
                this.$refs.outer.scrollTo(restore_pos, 0);
            }
            else {
                this.$refs.outer.scrollTo(0, restore_pos);
            }
            this.reference_element = null;
        }
    },
    mounted: function () {
        this.onscroll({target: this.$refs.outer}); // trigger a first-time preload
    },
}
</script>

<style lang="scss" scoped>
    .s4-infinite-list {
        display: flex;
        flex-direction: var(--s4-infinite-list-direction);
        flex: 1 1 0;
        overflow: scroll;
    }
</style>

// vim: set sw=4 ts=4 et indk= :
