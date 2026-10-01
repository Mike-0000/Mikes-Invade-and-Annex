#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Reads what tools/author_heli_paint_channels.py needs to know about a
//! helicopter, vanilla or modded, and writes it as JSON: the prefab's class
//! and ids, the materials its hull mesh shows, every slotted part with the
//! materials of its mesh, and the colours and numbers each material file sets.
//! It reads only; nothing is created or changed.
//!
//!   ArmaReforgerWorkbenchSteamDiag.exe -wbModule=ResourceManager -plugin=IA_HeliPaintSurvey -run
//!       -iaSurveyPrefab Prefabs/Vehicles/Helicopters/UH1H/UH1H.et,Prefabs/.../UH1H_armed.et
//!       -iaSurveyOut IA_HeliPaintSurvey.json
//!
//! The file lands in the Workbench profile folder. Feed it to the generator
//! with --adopt; docs/transport-pilot-progression.md has the whole procedure.
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter paint survey", wbModules: {"ResourceManager"})]
class IA_HeliPaintSurvey : WorkbenchPlugin
{
	protected static const string DEFAULT_OUT = "IA_HeliPaintSurvey.json";
	protected static const string MESH_CLASS = "MeshObject";
	protected static const string SLOT_CLASS = "SlotManagerComponent";

	protected ref array<ref Resource> m_aLoaded = {};
	// Every material a surveyed mesh shows, each once.
	protected ref array<ResourceName> m_aMaterials = {};
	protected int m_iFailures;

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		Print("[IA][HeliPaintSurvey] Run from the command line with -iaSurveyPrefab <path[,path]>; see the header of IA_HeliPaintSurvey.c.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		string paths;
		System.GetCLIParam("iaSurveyPrefab", paths);
		string outName = DEFAULT_OUT;
		System.GetCLIParam("iaSurveyOut", outName);
		if (outName.IsEmpty())
			outName = DEFAULT_OUT;

		ref array<string> prefabs = {};
		paths.Split(",", prefabs, true);
		if (prefabs.IsEmpty())
		{
			Print("[IA][HeliPaintSurvey] FAIL no -iaSurveyPrefab given", LogLevel.ERROR);
			Workbench.Exit(1);
			return;
		}

		ref array<string> airframes = {};
		string airframe;
		foreach (string path : prefabs)
		{
			airframe = SurveyAirframe(path);
			if (airframe.IsEmpty())
				m_iFailures = m_iFailures + 1;
			else
				airframes.Insert(airframe);
		}

		ref array<string> materials = {};
		foreach (ResourceName material : m_aMaterials)
		{
			materials.Insert(SurveyMaterial(material));
		}

		FileHandle file = FileIO.OpenFile("$profile:" + outName, FileMode.WRITE);
		if (!file)
		{
			Print("[IA][HeliPaintSurvey] FAIL cannot write $profile:" + outName, LogLevel.ERROR);
			Workbench.Exit(1);
			return;
		}

		file.WriteLine("{");
		file.WriteLine(" \"airframes\": [");
		WriteList(file, airframes);
		file.WriteLine(" ],");
		file.WriteLine(" \"materials\": {");
		WriteList(file, materials);
		file.WriteLine(" }");
		file.WriteLine("}");
		file.Close();

		Print(string.Format("[IA][HeliPaintSurvey] airframes=%1 materials=%2 failures=%3 file=%4", airframes.Count(), materials.Count(), m_iFailures, outName), LogLevel.NORMAL);
		if (m_iFailures == 0)
			Print("[IA][HeliPaintSurvey] PASS", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteList(notnull FileHandle file, notnull array<string> entries)
	{
		int count = entries.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (i + 1 < count)
				file.WriteLine(entries[i] + ",");
			else
				file.WriteLine(entries[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return one airframe as a JSON object, empty when the prefab does not load
	protected string SurveyAirframe(string path)
	{
		ResourceName prefab = path;
		if (!path.StartsWith("{"))
			prefab = Workbench.GetResourceName(path);

		BaseContainer source = Load(prefab);
		if (!source)
			return string.Empty;

		string text = "  {\n";
		text = text + EntityFields(source, prefab, "   ");

		ref array<string> parts = {};
		BaseContainer slotManager = FindComponent(source, SLOT_CLASS);
		BaseContainerList slots;
		if (slotManager)
			slots = slotManager.GetObjectArray("Slots");

		BaseContainer slot;
		BaseContainer part;
		ResourceName partPrefab;
		string partText;
		int slotCount;
		int s;
		if (slots)
			slotCount = slots.Count();
		for (s = 0; s < slotCount; s++)
		{
			slot = slots.Get(s);
			partPrefab = ResourceName.Empty;
			slot.Get("Prefab", partPrefab);
			if (partPrefab.IsEmpty())
				continue;

			part = Load(partPrefab);
			// A part with no mesh shows no paint.
			if (!part || !FindComponent(part, MESH_CLASS))
				continue;

			partText = "    {\n";
			partText = partText + string.Format("     \"slot\": %1,\n", Quote(slot.GetName()));
			partText = partText + string.Format("     \"slot_class\": %1,\n", Quote(slot.GetClassName()));
			partText = partText + EntityFields(part, partPrefab, "     ");
			partText = partText + "\n    }";
			parts.Insert(partText);
		}

		text = text + ",\n   \"parts\": [\n";
		int partCount = parts.Count();
		int p;
		for (p = 0; p < partCount; p++)
		{
			text = text + parts[p];
			if (p + 1 < partCount)
				text = text + ",";
			text = text + "\n";
		}
		text = text + "   ]\n  }";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Fields shared by a hull and a part: prefab, class, ids, mesh and the material of each mesh slot.
	protected string EntityFields(notnull BaseContainer source, ResourceName prefab, string indent)
	{
		string id;
		string meshId;
		string slotId;
		ReadIds(source, prefab, id, meshId, slotId);

		string text = string.Format("%1\"prefab\": %2,\n", indent, Quote(prefab));
		text = text + string.Format("%1\"class\": %2,\n", indent, Quote(source.GetClassName()));
		text = text + string.Format("%1\"id\": %2,\n", indent, Quote(id));
		text = text + string.Format("%1\"mesh_component\": %2,\n", indent, Quote(meshId));
		text = text + string.Format("%1\"slot_component\": %2,\n", indent, Quote(slotId));

		ResourceName mesh;
		BaseContainer meshComponent = FindComponent(source, MESH_CLASS);
		if (meshComponent)
			meshComponent.Get("Object", mesh);
		text = text + string.Format("%1\"object\": %2,\n", indent, Quote(mesh));
		text = text + string.Format("%1\"slots\": [\n", indent);

		ref array<string> names = {};
		ref array<ResourceName> materials = {};
		if (meshComponent)
			MeshSlots(meshComponent, mesh, names, materials);

		int count = names.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			text = text + string.Format("%1 {\"slot\": %2, \"material\": %3}", indent, Quote(names[i]), Quote(materials[i]));
			if (i + 1 < count)
				text = text + ",";
			text = text + "\n";
		}
		text = text + indent + "]";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Each material slot of a mesh, once, with the material the prefab shows on it: the one its
	//! MeshObject assigns, else the default the slot name carries as a GUID suffix.
	protected void MeshSlots(notnull BaseContainer meshComponent, ResourceName mesh, notnull array<string> names, notnull array<ResourceName> materials)
	{
		if (mesh.IsEmpty())
			return;

		Resource resource = Resource.Load(mesh);
		if (!resource || !resource.IsValid())
			return;

		VObject vobj = resource.GetResource().ToVObject();
		if (!vobj)
			return;

		BaseContainerList assigned = meshComponent.GetObjectArray("Materials");
		int assignedCount;
		if (assigned)
			assignedCount = assigned.Count();

		string slots[256];
		int count = vobj.GetMaterials(slots);
		BaseContainer entry;
		ResourceName material;
		string source;
		int cut;
		int i;
		int a;
		for (i = 0; i < count; i++)
		{
			// The list repeats a slot for each level of detail.
			if (names.Contains(slots[i]))
				continue;

			material = ResourceName.Empty;
			for (a = 0; a < assignedCount; a++)
			{
				entry = assigned.Get(a);
				source = string.Empty;
				entry.Get("SourceMaterial", source);
				if (source == slots[i])
					entry.Get("AssignedMaterial", material);
			}

			cut = slots[i].LastIndexOf("_");
			if (material.IsEmpty() && cut >= 0)
				material = ResolveGuid(slots[i].Substring(cut + 1, slots[i].Length() - cut - 1));

			names.Insert(slots[i]);
			materials.Insert(material);
			if (!material.IsEmpty() && !m_aMaterials.Contains(material))
				m_aMaterials.Insert(material);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return the full name of the resource with this GUID, empty when there is none
	protected ResourceName ResolveGuid(string guid)
	{
		if (guid.Length() != 16)
			return ResourceName.Empty;

		ResourceName byGuid = "{" + guid + "}";
		Resource resource = Resource.Load(byGuid);
		if (!resource || !resource.IsValid())
			return ResourceName.Empty;

		BaseContainer source = resource.GetResource().ToBaseContainer();
		if (!source)
			return ResourceName.Empty;
		return source.GetResourceName();
	}

	//------------------------------------------------------------------------------------------------
	//! The entity id of a prefab and the ids of its MeshObject and SlotManagerComponent. The
	//! container API does not give them, so they are read from the text of the prefab and of the
	//! prefabs it inherits; a component keeps the id of the prefab that first declared it.
	protected void ReadIds(notnull BaseContainer source, ResourceName prefab, out string id, out string meshId, out string slotId)
	{
		id = string.Empty;
		meshId = string.Empty;
		slotId = string.Empty;

		BaseContainer container = source;
		ResourceName name = prefab;
		FileHandle file;
		string line;
		bool first = true;
		while (container)
		{
			file = FileIO.OpenFile(name.GetPath(), FileMode.READ);
			if (file)
			{
				while (file.ReadLine(line) >= 0)
				{
					if (first && id.IsEmpty() && line.StartsWith(" ID \""))
						id = Between(line, "\"", "\"");
					if (meshId.IsEmpty() && line.StartsWith("  " + MESH_CLASS + " \"{"))
						meshId = Between(line, "{", "}");
					if (slotId.IsEmpty() && line.StartsWith("  " + SLOT_CLASS + " \"{"))
						slotId = Between(line, "{", "}");
				}
				file.Close();
			}

			first = false;
			container = container.GetAncestor();
			if (container)
				name = container.GetResourceName();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string Between(string line, string open, string close)
	{
		int start = line.IndexOf(open);
		if (start < 0)
			return string.Empty;

		start = start + open.Length();
		int end = line.IndexOfFrom(start, close);
		if (end < 0)
			return string.Empty;
		return line.Substring(start, end - start);
	}

	//------------------------------------------------------------------------------------------------
	//! \return one material as a JSON member: its class and every colour and number its file, or a
	//! file that one inherits, sets
	protected string SurveyMaterial(ResourceName material)
	{
		string text = string.Format("  %1: {", Quote(material));
		BaseContainer source = Load(material);
		if (!source)
		{
			m_iFailures = m_iFailures + 1;
			return text + "}";
		}

		text = text + string.Format("\"class\": %1, \"params\": {", Quote(source.GetClassName()));

		BaseContainer container;
		DataVarType type;
		string name;
		string color;
		float scalar;
		bool any;
		int count = source.GetNumVars();
		int i;
		for (i = 0; i < count; i++)
		{
			type = source.GetDataVarType(i);
			if (type != DataVarType.COLOR && type != DataVarType.SCALAR)
				continue;

			name = source.GetVarName(i);
			container = source;
			while (container && !container.IsVariableSetDirectly(name))
			{
				container = container.GetAncestor();
			}
			if (!container)
				continue;

			if (any)
				text = text + ", ";
			any = true;

			if (type == DataVarType.COLOR)
			{
				color = string.Empty;
				container.Get(name, color);
				text = text + string.Format("%1: %2", Quote(name), Quote(color));
			}
			else
			{
				scalar = 0;
				container.Get(name, scalar);
				text = text + string.Format("%1: %2", Quote(name), scalar.ToString());
			}
		}
		return text + "}}";
	}

	//------------------------------------------------------------------------------------------------
	protected BaseContainer FindComponent(notnull BaseContainer source, string className)
	{
		BaseContainerList components = source.GetObjectArray("components");
		if (!components)
			return null;

		BaseContainer component;
		int count = components.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			component = components.Get(i);
			if (component.GetClassName() == className)
				return component;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected BaseContainer Load(ResourceName name)
	{
		ref Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
		{
			Print("[IA][HeliPaintSurvey] Does not load: " + name, LogLevel.ERROR);
			return null;
		}

		m_aLoaded.Insert(resource);
		return resource.GetResource().ToBaseContainer();
	}

	//------------------------------------------------------------------------------------------------
	protected string Quote(string value)
	{
		value.Replace("\\", "\\\\");
		value.Replace("\"", "\\\"");
		return "\"" + value + "\"";
	}
}
#endif
