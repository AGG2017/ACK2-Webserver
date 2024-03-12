document.addEventListener("DOMContentLoaded", function () {
  fetch("api/info.json")
    .then((response) => response.json())
    .then((data) => {
      document.getElementById("totalMemory").textContent = data.total_mem;
      document.getElementById("freeMemory").textContent = data.free_mem;
      document.getElementById("freeMemoryPercentage").textContent =
        data.free_mem_per + "%";
      document.getElementById("cpuTotalUsage").textContent = data.cpu_use;
      document.getElementById("cpuUserUsage").textContent = data.cpu_usr_use;
      document.getElementById("cpuSystemUsage").textContent = data.cpu_sys_use;
      document.getElementById("cpuIdle").textContent = data.cpu_idle;
    })
    .catch((error) => console.error("Error fetching data:", error));

  fetch("api/webserver.json")
    .then((response) => response.json())
    .then((data) => {
      document.getElementById("printerModel").textContent = data.printer_model;
      document.getElementById("fwVersion").textContent = data.update_version;
      document.getElementById("unleashedUrl").textContent =
        data.mqtt_webui_url + "%";
    })
    .catch((error) => console.error("Error fetching data:", error));

  fetch("api/webserver.json")
    .then((response) => response.json())
    .then((data) => {
      const unleashedUrl = data.mqtt_webui_url;
      const unleashedLink = document.getElementById("unleashedLink");
      unleashedLink.href = unleashedUrl;
    })
    .catch((error) => console.error("Error fetching data:", error));
});
