importScripts('https://www.gstatic.com/firebasejs/8.2.5/firebase-app.js');
importScripts('https://www.gstatic.com/firebasejs/8.2.5/firebase-messaging.js');

const web_app_config = {
    apiKey: "AIzaSyAa8vGbPDQDOtF4cjKqYa_b99hK7KSPqBI",
    authDomain: "scooterfleet.firebaseapp.com",
    databaseURL: "https://scooterfleet.firebaseio.com",
    projectId: "scooterfleet",
    storageBucket: "scooterfleet.appspot.com",
    messagingSenderId: "355671533390",
    appId: "1:355671533390:web:dcfc7550ed03c6cc4ba01f"
};

firebase.initializeApp(web_app_config);

const messaging = firebase.messaging();

messaging.onBackgroundMessage(payload => {
    console.log("Message received in bg: " + JSON.stringify(payload));
    // https://stackoverflow.com/questions/65946708/javascript-service-worker-send-message-in-background
    //clients.matchAll({ includeUncontrolled: true }).then(clients => clients.forEach(client => client.postMessage(payload)));
    return Promise.reject();
});

// vim: set sw=4 ts=4 indk= et:
