<template>
    <div class="users">
        <s4-infinite-list :supply-items="gen_items" :max-items="19">
            <template v-slot:header>
                <v-list-item>
                    <v-list-item-content class="text-center">
                        <span>No preceding entries</span>
                    </v-list-item-content>
                </v-list-item>
            </template>

            <template v-slot:item="{item, idx}">
                <v-list-item two-line :class="'bg-' + pmod(idx, 2)" @click="edit_user(item)">
                    <v-list-item-content>
                        <v-list-item-title>{{ item.email }}</v-list-item-title>
                        <v-list-item-subtitle>Admin: {{ yesno(item.is_admin) }}</v-list-item-subtitle>
                        <v-list-item-subtitle>Technician: {{ yesno(item.is_technician) }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>
            </template>

            <template v-slot:footer>
                <v-list-item>
                    <v-list-item-content class="text-center">
                        <span>No further entries</span>
                    </v-list-item-content>
                </v-list-item>
            </template>
        </s4-infinite-list>

        <!--
        Input form for editing a user
        -->
        <v-dialog v-model="editing_user" max-width="90vw" modal>
            <v-card v-if="!!selected_user">
                <v-card-title class="text-h4">
                    <span>{{ selected_user.email }}</span>
                </v-card-title>

                <v-card-text>
                    <v-list-item>
                        <v-list-item-content>
                            <v-switch v-model="selected_user.is_admin" label="Admin"/>
                        </v-list-item-content>
                    </v-list-item>
                    <v-list-item>
                        <v-list-item-content>
                            <v-switch v-model="selected_user.is_technician" label="Technician"/>
                        </v-list-item-content>
                    </v-list-item>
                    <v-list-item class="text-center">
                        <v-list-item-content>
                            <v-btn color="error" @click="commit_selected_user" rounded>Change</v-btn>
                        </v-list-item-content>
                    </v-list-item>
                </v-card-text>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>
// @ is an alias to /src
import S4InfiniteList from "@/components/S4InfiniteList"

export default {
    name: "Users",
    components: {
        S4InfiniteList,
    },
    data () {
        return {
            editing_user: false,
            selected_user: null,
        };
    },
    methods: {
        yesno: function (x) {
            return x ? "YES" : "NO";
        },
        pmod: function (a, m) { // positive modulo
            return ((a % m) + m) % m;
        },
        gen_items: function (limit, num, before) {
            let query;
            if (limit !== null) {
                query = "/v0/user?" + (before ? "until" : "from") + "=" + encodeURIComponent(limit) + "&num=8";
            }
            else {
                query = "/v0/user?num=8";
            }   
            //console.log("gen_items(" + limit + ", " + num + ", " + before + ")");
            return this.$store.state.ax.get(query).then(response => {
                // for backward search: result is expected in a lausers-first order
                // for forward search: result is expected in an earliest-first order
                //console.log("response=" + JSON.stringify(response));
                let new_items = [...response.data];
                if (response.data.length < num) {
                    new_items.push(null);
                }
                return new_items;
            });
        },
        edit_user: function (u) {
            this.selected_user = u;
            this.editing_user = true;
        },
        commit_selected_user: function () {
            // FIXME: commit
            this.editing_user = false;
        },
    },
}
</script>

<style lang="scss" scoped>
    .users {
        height: 100%;
        display: flex;
        flex-direction: column;
    }

    .bg-0 {
        background-color: #ddf;
    }

    .bg-1 {
        background-color: #eef;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
