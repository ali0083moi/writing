// Game variables
let webSocket;
let playerId;
let playerName;
let players = {};
let adminX = 64;
let adminY = 32;
let spotlightRadius = 15;
let gameStarted = false;
let gameOver = false;
let adminWon = false;
let moving = false;
let myPlayer = null;
let gameContainer = document.getElementById("gameContainer");
let containerRect = gameContainer.getBoundingClientRect();
let containerWidth = 128; // Match ESP32 OLED screen dimensions
let containerHeight = 64; // Match ESP32 OLED screen dimensions

// Initialize when the page loads
window.addEventListener("load", () => {
  initializeGame();
});

// Setup WebSocket connection and game event listeners
function initializeGame() {
  // Create WebSocket connection
  const loc = window.location;
  const protocol = loc.protocol === "https:" ? "wss:" : "ws:";
  const wsUrl = `${protocol}//${loc.host}/ws`;

  webSocket = new WebSocket(wsUrl);

  webSocket.onopen = () => {
    console.log("WebSocket connection established");
    // Get player name from localStorage (set after login form submission)
    playerName =
      sessionStorage.getItem("playerName") ||
      new URLSearchParams(window.location.search).get("playerName") ||
      "Guest";
    registerPlayer(playerName);
  };

  webSocket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    handleWebSocketMessage(data);
  };

  webSocket.onclose = () => {
    console.log("WebSocket connection closed");
    document.getElementById("statusMessage").textContent =
      "Connection lost. Please refresh the page.";
  };

  // Setup game container for player movement
  setupPlayerMovement();

  // Setup start game button
  document.getElementById("startGameBtn").addEventListener("click", () => {
    sendMessage({ type: "startGame" });
  });

  // Handle window resize to keep game proportions correct
  window.addEventListener("resize", updateGameContainerDimensions);
  updateGameContainerDimensions();
}

// Register player with the server
function registerPlayer(name) {
  sendMessage({
    type: "register",
    name: name,
  });
}

// Send message to WebSocket server
function sendMessage(message) {
  if (webSocket && webSocket.readyState === WebSocket.OPEN) {
    webSocket.send(JSON.stringify(message));
  }
}

// Handle incoming WebSocket messages
function handleWebSocketMessage(data) {
  switch (data.type) {
    case "registered":
      playerId = data.id;
      playerName = data.name;
      console.log(`Registered as player: ${playerName} (ID: ${playerId})`);
      break;

    case "gameState":
      // Update game state
      gameStarted = data.gameStarted;
      adminX = data.adminX;
      adminY = data.adminY;
      spotlightRadius = data.spotlightRadius;
      gameOver = data.gameOver || false;
      adminWon = data.adminWon || false;

      // Update UI
      document.getElementById("timeRemaining").textContent = data.timeRemaining;
      document.getElementById("playerCount").textContent = data.players.length;

      // Update status message
      updateStatusMessage(gameStarted, gameOver, adminWon);

      // Update start game button
      const startGameBtn = document.getElementById("startGameBtn");
      startGameBtn.style.display =
        !gameStarted && data.players.length >= 2 ? "block" : "none";

      // Update player list
      updatePlayerList(data.players);

      // Update spotlight position
      updateSpotlight(adminX, adminY, spotlightRadius);

      // Update player positions
      updatePlayers(data.players);

      // Show game over message if needed
      if (gameOver) {
        showGameOverMessage(adminWon);
      }

      break;
  }
}

// Update status message based on game state
function updateStatusMessage(gameStarted, gameOver, adminWon) {
  const statusMessage = document.getElementById("statusMessage");

  if (gameOver) {
    statusMessage.textContent = adminWon
      ? "Game Over: Admin won! All players were spotted."
      : "Game Over: Players won! At least one player survived.";
  } else if (gameStarted) {
    statusMessage.textContent = "Game is running! Avoid the spotlight!";
  } else {
    statusMessage.textContent = "Waiting for the game to start...";
  }
}

// Update the player list UI
function updatePlayerList(playerData) {
  const playerList = document.getElementById("playerList");
  playerList.innerHTML = "";

  playerData.forEach((player) => {
    const playerItem = document.createElement("div");
    playerItem.className = `player-status ${
      player.active ? "active" : "eliminated"
    }`;
    playerItem.textContent = `${player.name} ${
      player.id === playerId ? "(You)" : ""
    }`;
    playerList.appendChild(playerItem);

    // Keep track of our player
    if (player.id === playerId) {
      myPlayer = player;
    }
  });
}

// Update the game container size proportionally
function updateGameContainerDimensions() {
  containerRect = gameContainer.getBoundingClientRect();

  // Scale factor is based on the 128x64 OLED screen dimensions
  const scaleFactor = Math.min(
    containerRect.width / containerWidth,
    containerRect.height / containerHeight
  );

  // Update container style
  gameContainer.style.width = `${containerWidth * scaleFactor}px`;
  gameContainer.style.height = `${containerHeight * scaleFactor}px`;
}

// Update the admin spotlight position
function updateSpotlight(x, y, radius) {
  const spotlight = document.getElementById("spotlight");

  // Calculate the scaled position
  const scaledX = (x / containerWidth) * containerRect.width;
  const scaledY = (y / containerHeight) * containerRect.height;
  const scaledRadius = (radius / containerWidth) * containerRect.width;

  spotlight.style.left = `${scaledX - scaledRadius}px`;
  spotlight.style.top = `${scaledY - scaledRadius}px`;
  spotlight.style.width = `${scaledRadius * 2}px`;
  spotlight.style.height = `${scaledRadius * 2}px`;
}

// Update all player positions and status
function updatePlayers(playerData) {
  // Remove existing player elements
  const existingPlayers = document.querySelectorAll(".player:not(#spotlight)");
  existingPlayers.forEach((el) => el.remove());

  // Add/update player elements
  playerData.forEach((player) => {
    let playerEl = document.querySelector(`.player[data-id="${player.id}"]`);

    if (!playerEl) {
      playerEl = document.createElement("div");
      playerEl.className = "player";
      playerEl.setAttribute("data-id", player.id);
      gameContainer.appendChild(playerEl);
    }

    // Calculate the scaled position
    const scaledX = (player.x / containerWidth) * containerRect.width;
    const scaledY = (player.y / containerHeight) * containerRect.height;

    playerEl.style.left = `${scaledX - 5}px`;
    playerEl.style.top = `${scaledY - 5}px`;

    // Update player classes based on state
    if (!player.active) {
      playerEl.classList.add("eliminated");
    } else if (player.isSpotted) {
      playerEl.classList.add("spotted");
    } else {
      playerEl.classList.remove("spotted");
    }

    // Add marker for self
    if (player.id === playerId) {
      playerEl.textContent = "Y";
      playerEl.style.fontWeight = "bold";
      playerEl.style.textAlign = "center";
      playerEl.style.fontSize = "8px";
      playerEl.style.color = "white";
    }
  });
}

// Setup player movement with mouse/touch events
function setupPlayerMovement() {
  // Mouse events
  gameContainer.addEventListener("mousedown", startMoving);
  document.addEventListener("mousemove", movePlayer);
  document.addEventListener("mouseup", stopMoving);

  // Touch events for mobile
  gameContainer.addEventListener("touchstart", (e) => {
    e.preventDefault();
    startMoving(e.touches[0]);
  });

  document.addEventListener("touchmove", (e) => {
    e.preventDefault();
    movePlayer(e.touches[0]);
  });

  document.addEventListener("touchend", stopMoving);
}

// Start moving player
function startMoving(e) {
  if (gameStarted && !gameOver && myPlayer && myPlayer.active) {
    moving = true;
    movePlayer(e);
  }
}

// Move player based on mouse/touch position
function movePlayer(e) {
  if (!moving || !myPlayer || !myPlayer.active) return;

  // Get game container position
  const rect = gameContainer.getBoundingClientRect();

  // Calculate position relative to game container
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  // Convert to OLED screen coordinates (128x64)
  const gameX = (x / rect.width) * containerWidth;
  const gameY = (y / rect.height) * containerHeight;

  // Ensure within bounds
  const boundedX = Math.max(0, Math.min(containerWidth, gameX));
  const boundedY = Math.max(0, Math.min(containerHeight, gameY));

  // Send movement to server
  sendMessage({
    type: "movePlayer",
    x: boundedX,
    y: boundedY,
  });
}

// Stop moving player
function stopMoving() {
  moving = false;
}

// Show game over message
function showGameOverMessage(adminWon) {
  const message = document.getElementById("gameMessage");
  message.textContent = adminWon
    ? "Admin Won! All players were spotted."
    : "Players Won! At least one survived.";
  message.classList.add("show");
}
