<template>
    <div class="account">
        <v-container fluid>
            
            <v-row dense>
                <v-text-field :value="$store.getters.auth.name" label="Name" :readonly="true"/>
            </v-row>
            
            <v-row dense>
                <v-text-field :value="$store.getters.auth.email" label="E-mail" :readonly="true"/>
            </v-row>
            
            <v-expansion-panels>
                <v-expansion-panel>
                    <v-expansion-panel-header class="text-h6">Payment cards</v-expansion-panel-header>
                    <v-expansion-panel-content>
                        <v-list>
                            <v-list-item v-for="(card, idx) in account.cards" :key="idx">
                                <v-list-item-action class="mx-0">
                                    <v-icon small>{{ card.to_delete ? "fas fa-times" : card.changed ? "fas fa-edit" : "" }}</v-icon>
                                </v-list-item-action>
                                <v-list-item-content>
                                    <span class="text-left">{{ card.name }}</span>
                                </v-list-item-content>
                                <v-list-item-action class="d-flex flex-row">
                                    <v-btn x-small icon @click="edit_card(idx)">
                                        <v-icon small color="primary">fas fa-edit</v-icon>
                                    </v-btn>
                                    <v-btn x-small icon @click="card.to_delete ^= true">
                                        <v-icon small color="error">fas fa-times</v-icon>
                                    </v-btn>
                                </v-list-item-action>
                            </v-list-item>
                        </v-list>
                        <v-btn rounded color="primary" @click="account.cards.push({name: 'New Card', changed: true, to_delete: false, })">
                            <v-icon small dark>fas fa-plus</v-icon>
                        </v-btn>
                    </v-expansion-panel-content>
                </v-expansion-panel>

            </v-expansion-panels>

            <v-divider class="py-2"></v-divider>
            
            <v-row dense>
                <v-col cols="6">
                     <v-btn icon color="primary" :disabled="!has_changed" @click="update_account"><v-icon>fas fa-save</v-icon></v-btn>
                </v-col>
                <v-col cols="6">
                     <v-btn @click="sign_out" icon color="error"><v-icon>fas fa-sign-out-alt</v-icon></v-btn>
                </v-col>
            </v-row>

        </v-container>

        <!--
        Alert for mocking the update step
        -->
        <v-dialog v-model="show_account_updated" max-width="60vw">
            <v-card>
                <v-card-title>
                    Account updated
                </v-card-title>
                <v-card-actions>
                    <v-row justify="center">
                        <v-btn color="success" @click="show_account_updated = false">OK</v-btn>
                    </v-row>
                </v-card-actions>
            </v-card>
        </v-dialog>

        <!--
        Input form for editing a card
        -->
        <v-dialog v-model="editing_card" max-width="90vw" modal>
            <v-card v-if="!!selected_card">
                <v-card-title class="text-h4">
                    <v-text-field v-model="selected_card.name" label="Name"></v-text-field>
                </v-card-title>

                <v-container>
                    <v-row>
                        <v-col cols="12">
                            <v-text-field v-model="selected_card.card_number" label="Number" placeholder="4 x 4 digits"></v-text-field>
                        </v-col>
                    </v-row>
                    <v-row>
                        <v-col cols="12">
                            <v-date-picker v-model="selected_card.expiry" type="month"></v-date-picker>
                        </v-col>
                    </v-row>
                    <v-row>
                        <v-col cols="12">
                            <v-text-field v-model="selected_card.cvc" :append-icon="show_cvc ? 'fas fa-eye' : 'fas fa-eye-slash'" :type="show_cvc ? 'text' : 'password'" label="CVC" @click:append="show_cvc ^= true"></v-text-field>
                        </v-col>
                    </v-row>
                    <v-row justify="center">
                        <v-col>
                            <v-btn color="error" @click="selected_card.changed = true; editing_card = false;" rounded>Change</v-btn>
                        </v-col>
                    </v-row>
                </v-container>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>

export default {
    name: "AccountPage",
    data () {
        return {
            has_changed: false,
            account: {
                cards: [
                    {
                        name: "1st Alpha Quadrant Bank",
                        card_number: "1234-5678-1526-3748",
                        expiry: "2364-11",
                        cvc: "123",
                        to_delete: false,
                        changed: false,
                    },
                    {
                        name: "Imperial Klingon Bank",
                        card_number: "1234-5678-1526-3748",
                        expiry: "2364-11",
                        cvc: "123",
                        to_delete: false,
                        changed: false,
                    },
                    {
                        name: "Ferengi Reserve",
                        card_number: "1234-5678-1526-3748",
                        expiry: "2364-11",
                        cvc: "123",
                        to_delete: false,
                        changed: false,
                    },
                ],
            },
            show_account_updated: false,

            editing_card: false,
            show_cvc: false,
            selected_card: null,
        };
    },
    watch: {
        account: {
            handler: function () {
                this.has_changed = true;
            },
            deep: true,
        },
    },
    methods: {
        update_account: function () {
            this.has_changed = false;
            this.show_account_updated = true;
        },
        edit_card: function (idx) {
            this.selected_card = this.account.cards[idx];
            this.show_cvc = false;
            this.editing_card = true;
        },
        sign_out: function () {
            console.log("signing out");
            this.$store.dispatch("sign_out").then(() => {
                    this.$store.commit("logged_out");
                    this.$router.push("/signin");
            });
        },
    },
    mounted: function () {
        this.$store.state.app_bar_info = "Account";
    },
    beforeDestroy: function () {
        this.$store.state.app_bar_info = "...";
    },
}

</script>

<style lang="scss">
    label.v-label:not(.v-label--active) {
        font-size: 1rem;
    }

    div[role="combobox"] {
        margin-top: 12px;

        input[type="text"] {
            margin-top: 6px;
        }
    }

</style>

<style lang="scss" scoped>
	.v-input {
        font-size: 1rem;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
