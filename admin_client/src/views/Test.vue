<template>
    <div class="test">
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
    </div>
</template>

<script>
// @ is an alias to /src
import { latLng } from "leaflet";
import S4InfiniteList from "@/components/S4InfiniteList"

export default {
    name: "TestPage",
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
        gen_items: function (limit, num, before) {
            let limit_time = (limit === null) ? Math.round(Date.now() / 1000) : limit.time;
            console.log("gen_items(" + limit + ", " + num + ", " + before + ")");
            let query = "/v0/unit/trace/Simulated?" + (before ? "until" : "from") + "=" + limit_time + "&num=8";
            // for backward search: result is expected in a latest-first order
            // for forward search: result is expected in an earliest-first order
            return this.$store.state.ax.get(query).then(response => {
                console.log("response=" + JSON.stringify(response));
                let new_items = [];

                response.data.forEach(u => {
                    new_items.push({
                        time: u.time,
                        unit: u.unit,
                        loc: latLng(u.lat, u.lon),
                        azi: u.azi,
                        spd: u.spd,
                        spdt: (u.spd * 3.6).toFixed(1),
                    });
                });
                if (response.data.length < num) {
                    new_items.push(null);
                }

                return new_items;
            });
        },
    },
}
</script>

<style lang="scss" scoped>
    .test {
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
