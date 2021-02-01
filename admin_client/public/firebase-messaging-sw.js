// Give the service worker access to Firebase Messaging.
// Note that you can only use Firebase Messaging here. Other Firebase libraries
// are not available in the service worker.
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
});

// vim: set sw=4 ts=4 indk= et:
