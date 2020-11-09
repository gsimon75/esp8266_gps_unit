import { latLng } from "leaflet";

var base_A = [ 0.004769, 0.00457 ];
var base_B = [ 0.004213, -0.005087 ];
const base_P = [ 25.112453, 55.168317 ];

const base_det = base_A[0] * base_B[1] - base_A[1] * base_B[0];

const base_Am = [ base_A[0] / base_det, base_A[1] / base_det ];
const base_Bm = [ base_B[0] / base_det, base_B[1] / base_det ];

function norm2latlng(p) {
    return latLng(base_P[0] + p[0]*base_A[0] + p[1]*base_B[0], base_P[1] + p[0]*base_A[1] + p[1]*base_B[1]);
}

function norm2lnglat(p) {
    return [
        base_P[1] + p[0]*base_A[1] + p[1]*base_B[1],
        base_P[0] + p[0]*base_A[0] + p[1]*base_B[0],
    ];
}

function scale(cx, cy, m, p) {
    return [ cx + (p[0] - cx) * m, cy + (p[1] - cy) * m ];
}

function transform_base(p_orig) {
    const p = [ p_orig.lat - base_P[0], p_orig.lng - base_P[1] ];

    var a =  p[0]*base_Bm[1] - p[1]*base_Bm[0];
    var b = -p[0]*base_Am[1] + p[1]*base_Am[0];

    a = (a >= 0) ? (a % 1) : 1 + (a % 1);
    b = (b >= 0) ? (b % 1) : 1 + (b % 1);

    return norm2latlng([a, b]);
}

const shop_coords = [
    [ [0.43, 0.45], [0.25, 0.4], [0.25, 0.25], [0.43, 0.3], ],
    [ [0.25, 0.4], [0.15, 0.45], [0.025, 0.4], [0.12, 0.3], [0.25, 0.25], ],
    [ [0.15, 0.45], [0.15, 0.55], [0.025, 0.6], [0, 0.5], [0.025, 0.4], ],
    [ [0.15, 0.55], [0.25, 0.6], [0.2, 0.7], [0.1, 0.75], [0.025, 0.6], ],
    [ [0.25, 0.6], [0.43, 0.55], [0.43, 0.7], [0.3, 0.73], [0.2, 0.7], ],
    [ [0.2, 0.7], [0.35, 0.75], [0.25, 0.9], [0.1, 0.75], ],
    [ [0.43, 0.7], [0.5, 0.75], [0.46, 0.85], [0.35, 0.75], [0.3, 0.73], ],
    [ [0.35, 0.75], [0.46, 0.85], [0.5, 0.9], [0.45, 1.0], [0.25, 0.9], ],
    [ [0.5, 0.75], [0.7, 0.8], [0.7, 0.9], [0.5, 0.9], [0.46, 0.85], ],
    [ [0.5, 0.9], [0.7, 0.9], [0.7, 0.95], [0.45, 1.0], ],
    [ [0.7, 0.8], [0.9, 0.75], [0.7, 0.95], ],
];

const shops = {
    type: "FeatureCollection",
    features: [],
};

for (let i in shop_coords) {
    
    shops.features.push({
        type: "Feature",
        geometry: {
            type: "Polygon",
            coordinates: [ shop_coords[i].map(c => norm2lnglat(scale(0.5, 0.5, 0.5, [ c[0], c[1] ]))) ],
        },
        properties: {
            id: "Shop A" + (1 + i),
            color: "#ffff00",
            fill: true,
            fillColor: "#ff0000",
            fillOpacity: 0.5,
        },
    });

    shops.features.push({
        type: "Feature",
        geometry: {
            type: "Polygon",
            coordinates: [ shop_coords[i].map(c => norm2lnglat(scale(0.5, 0.5, 0.5, [ 1 - c[0], 1 - c[1] ]))) ],
        },
        properties: {
            id: "Shop B" + (1 + i),
            color: "#00ffff",
            fill: true,
            fillColor: "#0000ff",
            fillOpacity: 0.5,
        },
    });

}

shops.features.push({
    type: "Feature",
    geometry: {
        type: "Polygon",
        coordinates: [ [
            norm2lnglat([0, 0]),
            norm2lnglat([1, 0]),
            norm2lnglat([1, 1]),
            norm2lnglat([0, 1]),
            norm2lnglat([0, 0]),
        ] ],
    },
    properties: {
        color: "#ffffff",
    },
});


export {
    norm2latlng,
    transform_base,
    shops,
};

// vim: set sw=4 ts=4 indk= et:

