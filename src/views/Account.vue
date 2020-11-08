<template>
    <div class="account">
        <v-container fluid>
            
            <v-row dense>
                <v-col cols="4"> <v-combobox v-model="account.title" label="Title" :items="S4DataSource.titles" placeholder="" dense></v-combobox> </v-col>
                <v-col cols="4"> <v-text-field v-model="account.given_name" label="Given name" placeholder=""></v-text-field> </v-col>
                <!--v-col cols="3"> <v-text-field v-model="account.middle_names" label="Middle names" placeholder=""></v-text-field> </v-col-->
                <v-col cols="4"> <v-text-field v-model="account.family_name" label="Family name" placeholder=""></v-text-field> </v-col>
            </v-row>
            
            <v-row dense>
                <v-col cols="2" class="my-auto text-left">Status:</v-col>
                <v-col cols="2" class="my-auto text-h6">{{ account.status }}</v-col>
                <v-col cols="2" class="my-auto text-left">Balance:</v-col>
                <v-col cols="2" class="my-auto text-h6">{{ account.balance }}</v-col>
                <v-col cols="4">
                    <v-btn outlined text rounded color="primary" @click="account.balance++">Top Up</v-btn>
                </v-col>
            </v-row>
            
            <v-divider class="py-2"></v-divider>
            
            <v-row dense>
                <v-col cols="2" class="my-auto text-left">Gender:</v-col>
                <v-col cols="2">
                     <v-btn outlined text rounded :color="account.is_male ? 'primary' : 'error'" @click="account.is_male ^= true">
                        <v-icon dark x-small> fas {{ account.is_male ? 'fa-mars' : 'fa-venus' }} </v-icon>
                     </v-btn>
                </v-col>
                <v-col cols="2"></v-col>
                <v-col cols="2" class="my-auto text-left">Born:</v-col>
                <v-col cols="4">
                    <v-dialog v-model="choosing_birth_date" width="500">
                        <template v-slot:activator="{ on, attrs }">
                            <v-btn outlined text rounded color="primary" v-bind="attrs" v-on="on">{{ account.date_of_birth }}</v-btn>
                        </template>
                        <v-card>
                            <v-card-title class="headline grey lighten-2">Date of Birth</v-card-title>
                            <v-card-text>
                                <v-date-picker v-model="account.date_of_birth" @change="choosing_birth_date = false"></v-date-picker>
                            </v-card-text>
                        </v-card>
                    </v-dialog>
                </v-col>
            </v-row>
            
            <v-row dense>
                <v-col cols="8"> <v-text-field v-model="account.email" label="E-mail" placeholder=""></v-text-field> </v-col>
                <v-col cols="4"> <v-text-field v-model="account.whatsapp" label="Whatsapp" placeholder=""></v-text-field> </v-col>
            </v-row>
            
            <v-row dense>
                <v-col cols="8"> <v-text-field v-model="account.skype" label="Skype" placeholder=""></v-text-field> </v-col>
                <v-col cols="4"> <v-text-field v-model="account.phone" label="Phone" placeholder=""></v-text-field> </v-col>
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

                <v-expansion-panel>
                    <v-expansion-panel-header class="text-h6">Addresses</v-expansion-panel-header>
                    <v-expansion-panel-content>
                        <v-list>
                            <v-list-item v-for="(address, idx) in account.addresses" :key="idx">
                                <v-list-item-action class="mx-0">
                                    <v-icon small>{{ address.to_delete ? "fas fa-times" : address.changed ? "fas fa-edit" : "" }}</v-icon>
                                </v-list-item-action>
                                <v-list-item-content>
                                    <span class="text-left">{{ address.name }}</span>
                                </v-list-item-content>
                                <v-list-item-action class="d-flex flex-row">
                                    <v-btn x-small icon @click="edit_address(idx)">
                                        <v-icon small color="primary">fas fa-edit</v-icon>
                                    </v-btn>
                                    <v-btn x-small icon @click="address.to_delete ^= true">
                                        <v-icon small color="error">fas fa-times</v-icon>
                                    </v-btn>
                                </v-list-item-action>
                            </v-list-item>
                        </v-list>
                        <v-btn rounded color="primary" @click="account.addresses.push({name: 'New Address', changed: true, to_delete: false, })">
                            <v-icon small dark>fas fa-plus</v-icon>
                        </v-btn>
                    </v-expansion-panel-content>
                </v-expansion-panel>

            </v-expansion-panels>

            <v-divider class="py-2"></v-divider>
            
            <v-row dense>
                <v-col cols="12"> <v-textarea outlined label="About myself" v-model="account.public_notes"></v-textarea> </v-col>
            </v-row>
            
            <v-row dense>
                <v-col cols="6">
                     <v-btn icon color="primary" :disabled="!has_changed" @click="update_account"><v-icon>fas fa-save</v-icon></v-btn>
                </v-col>
                <v-col cols="6">
                     <v-btn icon color="error"><v-icon>fas fa-sign-out-alt</v-icon></v-btn>
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

        <!--
        Input form for editing an address
        -->
        <v-dialog v-model="editing_address" max-width="90vw" modal>
            <v-card v-if="!!selected_address">
                <v-card-title class="text-h4">
                    <v-text-field v-model="selected_address.name" label="Name"></v-text-field>
                </v-card-title>

                <v-container>
                    <v-row>
                        <v-col cols="6">
                            <v-text-field v-model="selected_address.country" label="Country" placeholder=""></v-text-field>
                        </v-col>
                        <v-col cols="6">
                            <v-text-field v-model="selected_address.city" label="City" placeholder=""></v-text-field>
                        </v-col>
                    </v-row>
                    <v-row>
                        <v-col cols="6">
                            <v-text-field v-model="selected_address.street" label="Street" placeholder=""></v-text-field>
                        </v-col>
                        <v-col cols="6">
                            <v-text-field v-model="selected_address.house_nr" label="Nr" placeholder=""></v-text-field>
                        </v-col>
                    </v-row>
                    <v-row justify="center">
                        <v-col>
                            <v-btn color="error" @click="selected_address.changed = true; editing_address = false;" rounded>Change</v-btn>
                        </v-col>
                    </v-row>
                </v-container>
            </v-card>
        </v-dialog>

    </div>
</template>

<script>
import S4DataSource from "@/plugins/S4DataSource";

export default {
    name: "AccountPage",
    data () {
        return {
            S4DataSource,
            choosing_birth_date: false,
            has_changed: false,
            account: {
                status: "Active",
                balance: "22",
                title: "Cpt.",
                given_name: "Jean-Luc",
                middle_names: null,
                family_name: "Picard",
                date_of_birth: "2305-07-13",

                email: "picard@starfleet.federation.com",
                phone: "1-234-567890123",
                whatsapp: "1-234-567890123",
                skype: "jlp@FED-SFLT-EARTH",
                is_male: true,
                religion: null,
                allergies: 0,
                public_notes: "Space: the final frontier. These are the voyages of the starship Enterprise. Its continuing mission: to explore strange new worlds, to seek out new life and new civilizations, to boldly go where no one has gone before.",
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
                addresses: [
                    {
                        name: "Home",
                        country: "France",
                        city: "La Barre",
                        street: "Rue de l'Astres",
                        house_nr: "42",
                        to_delete: false,
                        changed: false,
                    },
                    {
                        name: "Work",
                        country: "USA",
                        city: "San Francisco",
                        street: "Starfleet HQ",
                        house_nr: "n/a",
                        to_delete: false,
                        changed: false,
                    },
                ],
            },
            show_account_updated: false,

            editing_card: false,
            show_cvc: false,
            selected_card: null,

            editing_address: false,
            selected_address: null,
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
        edit_address: function (idx) {
            this.selected_address = this.account.addresses[idx];
            this.editing_address = true;
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
