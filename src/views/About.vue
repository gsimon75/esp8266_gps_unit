<template>
    <div class="about">
        <s4-infinite-list :supply-items="gen_items" :max-items="19">
            <template v-slot:header>
                <span>Header</span>
            </template>

            <template v-slot:item="{idx, item}">
                <div :class="'qwer bg-' + ((item/10) % 2)">idx={{ idx }}, item={{ item }}{{ debug(idx, item) }}</div>
            </template>

            <template v-slot:footer>
                <span>Footer</span>
            </template>
        </s4-infinite-list>

        <s4-infinite-list :supply-items="gen_items" horizontal>
            <template v-slot:header>
                <span>Header</span>
            </template>

            <template v-slot:item="{idx, item}">
                <div :class="'asdf bg-' + ((item/10) % 2)">idx={{ idx }}, item={{ item }}{{ debug(idx, item) }}</div>
            </template>

            <template v-slot:footer>
                <span>Footer</span>
            </template>
        </s4-infinite-list>
    </div>
</template>

<script>
import S4InfiniteList from "@/components/S4InfiniteList"

export default {
    name: "AboutPage",
    components: {
        S4InfiniteList,
    },
    data () {
        return {
        };
    },
    methods: {
        debug: function (idx, item) {
            console.log("rendering idx=" + idx + ", item=" + item);
            return "";
        },
        gen_items: function (from, to) {
            console.log("gen_items(" + from + ", " + to + ")");
            let new_items = [];
            let null_to_the_end = false;

            if (from < -20) {
                new_items.push(null);
                from = -20;
            }
            if (20 < to) {
                null_to_the_end = true;
                to = 20;
            }
            for (let i = from; i < to; i++) {
                new_items.push(1010 * i);
            }
            if (null_to_the_end) {
                new_items.push(null);
            }
            return new_items;
        },
    },
}
</script>

<style lang="scss" scoped>
    .about {
        height: 100%;
        display: flex;
        flex-direction: column;
    }

    .qwer {
        height: 100px;
        width: 100%;
    }

    .asdf {
        width: 100px;
        height: 100%;
    }

    .bg-0 {
        background-color: #fee;
    }

    .bg-1, .bg--1 {
        background-color: #eef;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
