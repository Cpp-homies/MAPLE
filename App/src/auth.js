import { startInterval, stopInterval, clearData } from './index.js';

const baseUrl = "https://cloud.kovanen.io";


// Check if server is running on interval until it is
let serverCheckInterval;
function startServerCheckInterval() {
    serverCheckInterval = setInterval(() => {
        checkServer();
    }, 3000);
}

// Check if server is running
async function checkServer() {
    const serverStatusElement = document.getElementById("serverStatus");

    try {
        const response = await fetch(`${baseUrl}/`);
        if (response.ok) {
            console.log("Server is running");
            serverStatusElement.textContent = "Online";
            serverStatusElement.style.color = "green";
            clearInterval(serverCheckInterval);
            return true;
        } else {
            console.error("Server is not running");
            serverStatusElement.textContent = "Offline";
            serverStatusElement.style.color = "red";
            return false;
        }
    } catch (error) {
        console.error("Error fetching server status:", error);
        serverStatusElement.textContent = "Error";
        serverStatusElement.style.color = "red";
        return false;
    }
}


async function sha256(input) {
    const encoder = new TextEncoder();
    const data = encoder.encode(input);
    const digest = await crypto.subtle.digest('SHA-256', data);
    const array = new Uint8Array(digest);
    const hexCodes = Array.from(array).map((byte) => byte.toString(16).padStart(2, '0'));
    return hexCodes.join('');
}

async function register(event) {
    const registerUrl = `${baseUrl}/auth/register`;

    const usernameInput = document.querySelector(
        'input[name="register-username"]'
    );
    const passwordInput = document.querySelector('input[name="register-psw"]');

    const userId = usernameInput.value;
    const password = passwordInput.value;

    const hashedUserId = await sha256(userId);
    const hashedPassword = await sha256(password);

    try {
        const response = await fetch(registerUrl, {
            method: "PUT",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                user_id: hashedUserId,
                password: hashedPassword,
            }),
        });

        if (response.ok) {
            console.log("User registered successfully");
            login_fetch(hashedUserId, hashedPassword); // Log in the user
            closeRegister(); // Close the register popup
            showSuccessPopup("Registered and logged in successfully"); // Show success popup
            showUserLogout(userId); // Show user ID and logout button
        } else {
            console.error("Error registering user:", response.status);
            showErrorPopup("Error registering user:", response.status);
        }
    } catch (error) {
        console.error("Error registering user:", error);
        showErrorPopup("Error registering user:", error);
    }
}

async function login(event) {
    const usernameInput = document.querySelector('input[name="username"]');
    const passwordInput = document.querySelector('input[name="psw"]');

    const userId = usernameInput.value;
    const password = passwordInput.value;

    const hashedUserId = await sha256(userId);
    const hashedPassword = await sha256(password);

    login_fetch(hashedUserId, hashedPassword, userId);
}

export async function login_fetch(userId, password, userId_text = null) {
    const loginUrl = `${baseUrl}/auth/login`;

    console.log(userId, password)

    try {
        const response = await fetch(
            `${loginUrl}?user_id=${userId}&password=${password}`, { credentials: 'include' }
        );
        if (response.ok) {
            const data = await response.json();
            if (data.status === 1) {
                console.log("User logged in successfully");
                localStorage.setItem("user_id", userId); // Save the user_id in localStorage
                localStorage.setItem("user_id_text", userId_text) // Save the user_id_text in localStorage
                localStorage.setItem("token", password); // Save the token in localStorage
                console.log(userId, password);
                closeLogin(); // Close the login popup
                showSuccessPopup("Logged in successfully"); // Show success popup
                showUserLogout(userId_text); // Show user ID and logout button
                startInterval();
            } else if (data.status === 0) {
                console.error("Authentication failed: Incorrect password");
            }
        } else if (response.status === 404) {
            console.error("Authentication failed: User not found");
            showErrorPopup("User not found");
        } else {
            console.error("Error logging in user:", response.status);
            showErrorPopup("Error logging in user");
        }
    } catch (error) {
        console.error("Error logging in user:", error);
        showErrorPopup("Error logging in user");
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

function logout(event) {
    // Remove user data from localStorage
    localStorage.removeItem("user_id");
    localStorage.removeItem("user_id_text");
    localStorage.removeItem("token");

    // Stop any running interval
    stopInterval();

    // Hide user ID and logout button
    hideUserLogout();

    // Clear data from the page
    clearData();

    // Show success popup
    showSuccessPopup("Logged out successfully");

    // Reload the page
    location.reload();

}


function showSuccessPopup(message) {
    const successPopup = document.getElementById("success-popup");
    const successMessage = document.getElementById("successMessage");

    successMessage.textContent = message;
    successPopup.style.display = "block";
    setTimeout(() => {
        hideSuccessPopup();
    }, 3000);
}

function hideSuccessPopup() {
    const successPopup = document.getElementById("success-popup");
    successPopup.style.display = "none";
}

function showErrorPopup(message) {
    const errorPopup = document.getElementById("error-popup");
    const errorMessage = document.getElementById("errorMessage");

    errorMessage.textContent = message;
    errorPopup.style.display = "block";
    setTimeout(() => {
        hideErrorPopup();
    }, 3000);
}

function hideErrorPopup() {
    const errorPopup = document.getElementById("error-popup");
    errorPopup.style.display = "none";
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

document.getElementById("closeSuccessPopup").addEventListener('click', (event) => {
    hideSuccessPopup();
});
document.getElementById("closeErrorPopup").addEventListener('click', (event) => {
    hideErrorPopup();
});

// Check if server is running after page has loaded
window.addEventListener("load", (event) => {
    startServerCheckInterval();
});