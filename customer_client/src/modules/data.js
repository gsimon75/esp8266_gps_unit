import Vue from "vue";
import { latLng } from "leaflet";
import { EventBus } from "@/modules/event-bus";


export default {
    namespaced: true,
    state: {
        db: null,
        stations: {},
        units: {},
    },
    getters: {
        my_units: state => Object.values(state.units).filter(u => u.status == "in_use"),
        available_units: state => Object.values(state.units).filter(u => u.status == "available"),
    },
    mutations: {
    },
    actions: {
        open_db(context) {
            window.sqlitePlugin.openDatabase({
                    name: "saga.db",
                    location: "default",
                    createFromLocation: 1,
                },
                db => {
                    console.log("db opened OK");
                    context.state.db = db;
                },
                error => {
                    alert("db open failed: " + error);
                    context.state.db = null;
                }
            );
        },
        fetch_stations(context) {
            console.log("Fetching stations");
            context.rootState.ax.get("/v0/station").then(response => {
                // response.status == 200
                let newstations = {}
                let n = 0;
                for (var st of response.data) {
                    // {"_id":"6016822fcbe5bf1db53ae6c2","id":3825891566,"lat":25.1850197,"lon":55.2652917,"name":"The Health Spot Cafe","capacity":14,"in_use":0}
                    newstations[st._id] = {
                        name: st.name,
                        free: st.capacity - st.in_use,
                        ready: st.in_use,
                        charging: 0, // TODO: distinguish charging vs. ready
                        loc: latLng(st.lat, st.lon),
                    };
                    n++;
                }
                console.log("Fetched " + n + " stations");
                context.state.stations = newstations;
            });
        },
        fetch_units(context) {
            console.log("Fetching my units");
            context.rootState.ax.get("/v0/unit").then(response => {
                // response.status == 200
                let newunits = {}
                let n = 0;
                for (var u of response.data) {
                    newunits[u.unit] = {
                        ...u,
                        loc: latLng(u.lat, u.lon),
                        spdt: (u.spd * 3.6).toFixed(1),
                    };
                    n++;
                }
                console.log("Fetched " + n + " units");
                context.state.units = newunits;
            });
        },
        update_unit(context, u) {
            console.log("Updating a unit: " + JSON.stringify(u));
            if ((u.lat !== undefined) && (u.lon !== undefined)) {
                u.loc = latLng(u.lat, u.lon);
            }
            if (u.spd !== undefined) {
                u.spdt = (u.spd * 3.6).toFixed(1);
            }
            const uu = context.state.units[u.unit];
            if (!uu) {
                Vue.set(context.state.units, u.unit, { ...u });
            }
            else {
                for (let k in u) {
                    if (u[k] != uu[k]) {
                        Vue.set(uu, k, u[k]);
                    }
                }
            }
        },
    },
};

// vim: set sw=4 ts=4 indk= et:
