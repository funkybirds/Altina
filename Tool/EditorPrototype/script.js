const prototypeData = {
    hierarchy: [
        {
            id: "root-scene",
            label: "RootScene",
            meta: "Actor",
            kind: "Actor",
            path: "WorldRoot / RootScene",
            fields: [
                ["Transform", "0.0, 0.0, 0.0"],
                ["Mobility", "Static"],
                ["Children", "6"]
            ],
            children: [
                {
                    id: "camera-rig",
                    label: "CameraRig",
                    meta: "Actor",
                    kind: "Actor",
                    path: "WorldRoot / Gameplay / CameraRig",
                    fields: [
                        ["Lens", "35 mm"],
                        ["Focus", "Player start"],
                        ["Post", "Exposure Volume A"]
                    ],
                    children: [
                        {
                            id: "camera-component",
                            label: "CameraComponent",
                            meta: "Component",
                            kind: "Component",
                            path: "WorldRoot / Gameplay / CameraRig / CameraComponent",
                            fields: [
                                ["FOV", "72 deg"],
                                ["Near Clip", "0.05 m"],
                                ["Jitter", "Enabled"]
                            ]
                        }
                    ]
                },
                {
                    id: "sun-light",
                    label: "SunLight",
                    meta: "Light",
                    kind: "Actor",
                    path: "WorldRoot / Lighting / SunLight",
                    fields: [
                        ["Intensity", "10.4 lux"],
                        ["Temperature", "5900 K"],
                        ["Shadows", "Virtual"]
                    ]
                },
                {
                    id: "ruin-cluster",
                    label: "RuinCluster",
                    meta: "Actor",
                    kind: "Actor",
                    path: "WorldRoot / Environment / RuinCluster",
                    fields: [
                        ["Meshes", "14"],
                        ["Nanite", "Prototype"],
                        ["Streaming", "Resident"]
                    ]
                }
            ]
        }
    ],
    assets: {
        "Assets / Environments": {
            items: [
                { id: "forest-rocks", type: "Folder", name: "ForestRocks", meta: "12 items" },
                { id: "ruin-pillars", type: "Folder", name: "RuinPillars", meta: "8 items" },
                { id: "fog-material", type: "File", name: "M_FogLayer", meta: "Material" },
                { id: "stone-albedo", type: "File", name: "T_StoneAlbedo", meta: "Texture2D" },
                { id: "sky-cubemap", type: "File", name: "SkyMorning", meta: "Cubemap" },
                { id: "hero-ruin", type: "File", name: "SM_HeroRuin", meta: "StaticMesh" }
            ]
        },
        "Assets / Environments / ForestRocks": {
            items: [
                { id: "rock-a", type: "File", name: "SM_Rock_A", meta: "StaticMesh" },
                { id: "rock-b", type: "File", name: "SM_Rock_B", meta: "StaticMesh" },
                { id: "rock-c", type: "File", name: "SM_Rock_C", meta: "StaticMesh" },
                { id: "rock-mat", type: "File", name: "M_RockSurface", meta: "Material" }
            ]
        },
        "Assets / Materials": {
            items: [
                { id: "master-lit", type: "File", name: "M_MasterLit", meta: "Material" },
                { id: "water-foam", type: "File", name: "M_WaterFoam", meta: "Material" },
                { id: "post-bloom", type: "File", name: "PP_BloomSoft", meta: "PostProcess" }
            ]
        }
    },
    assetTree: [
        {
            id: "assets-root",
            label: "Assets",
            path: "Assets / Environments",
            children: [
                {
                    id: "env",
                    label: "Environments",
                    path: "Assets / Environments",
                    children: [
                        {
                            id: "forest-rocks",
                            label: "ForestRocks",
                            path: "Assets / Environments / ForestRocks"
                        }
                    ]
                },
                {
                    id: "materials",
                    label: "Materials",
                    path: "Assets / Materials"
                }
            ]
        }
    ],
    output: [
        { time: "09:41", level: "Info", text: "Editor shell booted with light prototype theme." },
        { time: "09:41", level: "Info", text: "Viewport preview seeded with environment scene." },
        { time: "09:42", level: "Warn", text: "Asset thumbnails are placeholder cards for the prototype." },
        { time: "09:42", level: "Info", text: "Hierarchy and asset selections update inspector details." }
    ]
};

const state = {
    selectedHierarchy: "camera-rig",
    selectedAssetTreePath: "Assets / Environments",
    selectedAssetId: "hero-ruin",
    expandedHierarchy: new Set(["root-scene", "camera-rig"]),
    expandedAssets: new Set(["assets-root", "env"]),
    activeMenu: null
};

const elements = {
    hierarchyTree: document.getElementById("hierarchy-tree"),
    inspectorTitle: document.getElementById("inspector-title"),
    inspectorKind: document.getElementById("inspector-kind"),
    inspectorPath: document.getElementById("inspector-path"),
    inspectorFields: document.getElementById("inspector-fields"),
    viewportSelection: document.getElementById("viewport-selection"),
    assetTree: document.getElementById("asset-tree"),
    assetGrid: document.getElementById("asset-grid"),
    assetFolderNote: document.getElementById("asset-folder-note"),
    outputFeed: document.getElementById("output-feed"),
    workspace: document.getElementById("workspace")
};

function flattenHierarchy(nodes, depth = 0, list = []) {
    nodes.forEach((node) => {
        list.push({ node, depth });
        if (node.children && state.expandedHierarchy.has(node.id)) {
            flattenHierarchy(node.children, depth + 1, list);
        }
    });
    return list;
}

function findHierarchyNode(nodes, id) {
    for (const node of nodes) {
        if (node.id === id) {
            return node;
        }
        if (node.children) {
            const found = findHierarchyNode(node.children, id);
            if (found) {
                return found;
            }
        }
    }
    return null;
}

function renderHierarchy() {
    const items = flattenHierarchy(prototypeData.hierarchy);
    elements.hierarchyTree.innerHTML = "";

    items.forEach(({ node, depth }) => {
        const item = document.createElement("button");
        item.className = "tree-item";
        if (state.selectedHierarchy === node.id) {
            item.classList.add("is-selected");
        }
        item.style.paddingLeft = `${12 + depth * 22}px`;

        const hasChildren = Array.isArray(node.children) && node.children.length > 0;
        const twist = document.createElement("span");
        twist.className = "tree-item__twist";
        twist.textContent = hasChildren ? (state.expandedHierarchy.has(node.id) ? "-" : "+") : "";
        item.appendChild(twist);

        const label = document.createElement("span");
        label.className = "tree-item__label";
        label.textContent = node.label;
        item.appendChild(label);

        const meta = document.createElement("span");
        meta.className = "tree-item__meta";
        meta.textContent = node.meta;
        item.appendChild(meta);

        item.addEventListener("click", () => {
            if (hasChildren) {
                if (state.expandedHierarchy.has(node.id)) {
                    state.expandedHierarchy.delete(node.id);
                } else {
                    state.expandedHierarchy.add(node.id);
                }
            }
            state.selectedHierarchy = node.id;
            syncInspector();
            renderHierarchy();
        });

        elements.hierarchyTree.appendChild(item);
    });
}

function syncInspector() {
    const node = findHierarchyNode(prototypeData.hierarchy, state.selectedHierarchy);
    if (!node) {
        return;
    }

    elements.inspectorTitle.textContent = node.label;
    elements.inspectorKind.textContent = node.kind;
    elements.inspectorPath.textContent = node.path;
    elements.viewportSelection.textContent = node.path.replace("WorldRoot / ", "");
    elements.inspectorFields.innerHTML = "";

    node.fields.forEach(([name, value]) => {
        const row = document.createElement("div");
        row.className = "field-row";

        const label = document.createElement("span");
        label.className = "field-label";
        label.textContent = name;
        row.appendChild(label);

        const content = document.createElement("span");
        content.className = "field-value";
        content.textContent = value;
        row.appendChild(content);

        elements.inspectorFields.appendChild(row);
    });
}

function flattenAssetTree(nodes, depth = 0, list = []) {
    nodes.forEach((node) => {
        list.push({ node, depth });
        if (node.children && state.expandedAssets.has(node.id)) {
            flattenAssetTree(node.children, depth + 1, list);
        }
    });
    return list;
}

function renderAssetTree() {
    const items = flattenAssetTree(prototypeData.assetTree);
    elements.assetTree.innerHTML = "";

    items.forEach(({ node, depth }) => {
        const item = document.createElement("button");
        item.className = "asset-tree-item";
        if (state.selectedAssetTreePath === node.path) {
            item.classList.add("is-selected");
        }
        item.style.paddingLeft = `${12 + depth * 22}px`;

        const hasChildren = Array.isArray(node.children) && node.children.length > 0;
        const twist = document.createElement("span");
        twist.className = "asset-tree-item__twist";
        twist.textContent = hasChildren ? (state.expandedAssets.has(node.id) ? "-" : "+") : "";
        item.appendChild(twist);

        const label = document.createElement("span");
        label.className = "tree-item__label";
        label.textContent = node.label;
        item.appendChild(label);

        item.addEventListener("click", () => {
            if (hasChildren) {
                if (state.expandedAssets.has(node.id)) {
                    state.expandedAssets.delete(node.id);
                } else {
                    state.expandedAssets.add(node.id);
                }
            }
            state.selectedAssetTreePath = node.path;
            state.selectedAssetId = null;
            renderAssetTree();
            renderAssetGrid();
        });

        elements.assetTree.appendChild(item);
    });
}

function currentAssetItems() {
    return prototypeData.assets[state.selectedAssetTreePath] || prototypeData.assets["Assets / Environments"];
}

function renderAssetGrid() {
    const folder = currentAssetItems();
    elements.assetFolderNote.textContent = state.selectedAssetTreePath;
    elements.assetGrid.innerHTML = "";

    folder.items.forEach((itemData) => {
        const item = document.createElement("button");
        item.className = "asset-card";
        if (state.selectedAssetId === itemData.id) {
            item.classList.add("is-selected");
        }

        const icon = document.createElement("span");
        icon.className = "asset-card__icon";
        icon.textContent = itemData.type === "Folder" ? "DIR" : "FILE";
        item.appendChild(icon);

        const title = document.createElement("strong");
        title.textContent = itemData.name;
        item.appendChild(title);

        const meta = document.createElement("span");
        meta.className = "asset-card__meta";
        meta.textContent = itemData.meta;
        item.appendChild(meta);

        item.addEventListener("click", () => {
            state.selectedAssetId = itemData.id;
            renderAssetGrid();
        });

        if (itemData.type === "Folder") {
            item.addEventListener("dblclick", () => {
                const nested = Object.keys(prototypeData.assets).find((key) => key.endsWith(itemData.name));
                if (nested) {
                    state.selectedAssetTreePath = nested;
                    renderAssetTree();
                    renderAssetGrid();
                }
            });
        }

        elements.assetGrid.appendChild(item);
    });
}

function renderOutput() {
    elements.outputFeed.innerHTML = "";
    prototypeData.output.forEach((entry) => {
        const row = document.createElement("div");
        row.className = "log-entry";

        const time = document.createElement("span");
        time.className = "log-entry__time";
        time.textContent = entry.time;
        row.appendChild(time);

        const level = document.createElement("span");
        level.className = "log-entry__level";
        if (entry.level === "Warn") {
            level.classList.add("is-warn");
        }
        level.textContent = entry.level;
        row.appendChild(level);

        const text = document.createElement("span");
        text.textContent = entry.text;
        row.appendChild(text);

        elements.outputFeed.appendChild(row);
    });
}

function setupTabs() {
    document.querySelectorAll(".panel-tabs").forEach((group) => {
        group.addEventListener("click", (event) => {
            const button = event.target.closest(".panel-tab");
            if (!button) {
                return;
            }

            group.querySelectorAll(".panel-tab").forEach((tab) => tab.classList.remove("is-active"));
            button.classList.add("is-active");

            const panelBody = group.nextElementSibling;
            panelBody.querySelectorAll(".tab-panel").forEach((panel) => panel.classList.remove("is-active"));
            const target = panelBody.querySelector(`#${button.dataset.tabTarget}`);
            if (target) {
                target.classList.add("is-active");
            }
        });
    });
}

function setupMenus() {
    const buttons = document.querySelectorAll(".menu-button");
    const menus = document.querySelectorAll(".menu-dropdown");

    function closeMenus() {
        state.activeMenu = null;
        buttons.forEach((button) => button.classList.remove("is-open"));
        menus.forEach((menu) => menu.classList.remove("is-open"));
    }

    buttons.forEach((button) => {
        button.addEventListener("click", (event) => {
            event.stopPropagation();
            const targetId = button.dataset.menuTarget;
            const isOpen = state.activeMenu === targetId;
            closeMenus();
            if (!isOpen) {
                state.activeMenu = targetId;
                button.classList.add("is-open");
                document.getElementById(targetId).classList.add("is-open");
            }
        });
    });

    document.addEventListener("click", (event) => {
        if (!event.target.closest(".menu-bar")) {
            closeMenus();
        }
    });
}

function setupSplitters() {
    const constraints = {
        left: { variable: "--left-width", min: 16, max: 32 },
        right: { variable: "--right-width", min: 18, max: 32 },
        bottom: { variable: "--bottom-height", min: 22, max: 42 },
        asset: { variable: "--asset-tree-width", min: 22, max: 48 }
    };

    document.querySelectorAll("[data-splitter]").forEach((splitter) => {
        splitter.addEventListener("pointerdown", (event) => {
            if (window.innerWidth <= 1100) {
                return;
            }

            const key = splitter.dataset.splitter;
            const rule = constraints[key];
            if (!rule) {
                return;
            }

            splitter.classList.add("is-dragging");
            splitter.setPointerCapture(event.pointerId);

            const onMove = (moveEvent) => {
                const rect = elements.workspace.getBoundingClientRect();
                let percentage = 0;

                if (key === "left") {
                    percentage = ((moveEvent.clientX - rect.left) / rect.width) * 100;
                } else if (key === "right") {
                    percentage = ((rect.right - moveEvent.clientX) / rect.width) * 100;
                } else if (key === "bottom") {
                    percentage = ((rect.bottom - moveEvent.clientY) / rect.height) * 100;
                } else if (key === "asset") {
                    const assetWorkspace = splitter.parentElement.getBoundingClientRect();
                    percentage = ((moveEvent.clientX - assetWorkspace.left) / assetWorkspace.width) * 100;
                }

                percentage = Math.min(rule.max, Math.max(rule.min, percentage));
                document.documentElement.style.setProperty(rule.variable, `${percentage}%`);
            };

            const onUp = () => {
                splitter.classList.remove("is-dragging");
                splitter.removeEventListener("pointermove", onMove);
                splitter.removeEventListener("pointerup", onUp);
                splitter.removeEventListener("pointercancel", onUp);
            };

            splitter.addEventListener("pointermove", onMove);
            splitter.addEventListener("pointerup", onUp);
            splitter.addEventListener("pointercancel", onUp);
        });
    });
}

function initialize() {
    setupTabs();
    setupMenus();
    setupSplitters();
    renderHierarchy();
    syncInspector();
    renderAssetTree();
    renderAssetGrid();
    renderOutput();
}

initialize();
