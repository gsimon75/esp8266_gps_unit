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
    },
};

// vim: set sw=4 ts=4 indk= et:
