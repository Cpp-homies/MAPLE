import { startInterval, stopInterval, clearData } from './index.js';

const baseUrl = "https://cloud.kovanen.io";

console.log("Auth.js loaded");

async function register(event) {
    const registerUrl = `${baseUrl}/auth/register`;

    const usernameInput = document.querySelector(
        'input[name="register-username"]'
    );
    const passwordInput = document.querySelector('input[name="register-psw"]');

    const userId = usernameInput.value;
    const password = passwordInput.value;

    try {
        const response = await fetch(registerUrl, {
            method: "PUT",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                user_id: userId,
                password: password,
            }),
        });

        if (response.ok) {
            console.log("User registered successfully");
            login_fetch(userId, password); // Log in the user
            closeRegister(); // Close the register popup
            showSuccessPopup("Registered and logged in successfully"); // Show success popup
            showUserLogout(userId); // Show user ID and logout button
        } else {
            console.error("Error registering user:", response.status);
        }
    } catch (error) {
        console.error("Error registering user:", error);
    }
}

async function login(event) {
    const usernameInput = document.querySelector('input[name="username"]');
    const passwordInput = document.querySelector('input[name="psw"]');

    const userId = usernameInput.value;
    const password = passwordInput.value;

    login_fetch(userId, password);
}

export async function login_fetch(userId, password) {
    const loginUrl = `${baseUrl}/auth/login`;

    try {
        const response = await fetch(
            `${loginUrl}?user_id=${userId}&password=${password}`, { credentials: 'include' }
        );
        if (response.ok) {
            const data = await response.json();
            if (data.status === 1) {
                console.log("User logged in successfully");
                localStorage.setItem("user_id", userId); // Save the user_id in localStorage
                localStorage.setItem("token", password); // Save the token in localStorage
                closeLogin(); // Close the login popup
                showSuccessPopup("Logged in successfully"); // Show success popup
                showUserLogout(userId); // Show user ID and logout button
                startInterval();
            } else if (data.status === 0) {
                console.error("Authentication failed: Incorrect password");
            }
        } else if (response.status === 404) {
            console.error("Authentication failed: User not found");
        } else {
            console.error("Error logging in user:", response.status);
        }
    } catch (error) {
        console.error("Error logging in user:", error);
    }
}

function showUserLogout(userId) {
    const userLogoutContainer = document.getElementById("userLogoutContainer");
    const displayUserId = document.getElementById("displayUserId");

    displayUserId.textContent = userId;
    userLogoutContainer.style.display = "flex";

    // Hide register and login buttons
    document.getElementById("register").style.display = "none";
    document.getElementById("openLoginPopup").style.display = "none";
}

function hideUserLogout() {
    const userLogoutContainer = document.getElementById("userLogoutContainer");

    userLogoutContainer.style.display = "none";

    // Show register and login buttons
    document.getElementById("register").style.display = "inline-block";
    document.getElementById("openLoginPopup").style.display = "inline-block";
}

function logout() {
    // Remove user data from localStorage
    localStorage.removeItem("user_id");
    localStorage.removeItem("token");

    // Stop any running interval
    stopInterval();

    // Hide user ID and logout button
    hideUserLogout();

    // Clear the displayed data
    clearData();
}


function showSuccessPopup(message) {
    const successPopup = document.getElementById("successPopup");
    const successMessage = document.getElementById("successMessage");

    successMessage.textContent = message;
    successPopup.style.display = "block";
    setTimeout(() => {
        hideSuccessPopup();
    }, 10000);
}

function hideSuccessPopup() {
    const successPopup = document.getElementById("successPopup");
    successPopup.style.display = "none";
}


document.getElementById("register-submit").addEventListener("click", (event) => {
    register(event);
});
document.getElementById("login-submit").addEventListener('click', (event) => {
    login(event);
});
document.getElementById("logout").addEventListener('click', (event) => {
    logout(event);
});