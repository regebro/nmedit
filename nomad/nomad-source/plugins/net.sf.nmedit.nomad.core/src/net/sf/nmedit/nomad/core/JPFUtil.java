/* Copyright (C) 2006 Christian Schneider
 * 
 * This file is part of Nomad.
 * 
 * Nomad is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Nomad is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Nomad; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Created on Nov 23, 2006
 */
package net.sf.nmedit.nomad.core;

import java.io.File;
import java.security.CodeSource;
import java.net.URISyntaxException;
import java.net.URL;

import org.java.plugin.PathResolver;
import org.java.plugin.Plugin;
import org.java.plugin.PluginManager;
import org.java.plugin.registry.PluginDescriptor;

public class JPFUtil
{
 
    private static PluginManager pluginManager = null;

    static void setPluginManager(PluginManager pm)
    {
        pluginManager = pm;
    }
    
    public static PluginManager getPluginManager()
    {
        return pluginManager;
    }

    public static File getPluginDirectory(Object anObject)
    {
        Plugin plugin = pluginManager.getPluginFor(anObject);
        if (plugin != null)
            return getDirectoryFromLocation(plugin.getDescriptor().getLocation());

        CodeSource source = anObject.getClass().getProtectionDomain().getCodeSource();
        if (source == null)
            return null;

        return getDirectoryFromLocation(source.getLocation());
    }

    private static File getDirectoryFromLocation(URL url)
    {
        File dir = null;
        try
        {
            File location = new File(url.toURI());

            if (location.isFile())
                location = location.getParentFile();

            if (location == null)
                return null;

            String name = location.getName();
            if ("classes".equals(name) || "lib".equals(name) || "data".equals(name) || "codecs".equals(name))
                location = location.getParentFile();

            dir = location;
        }
        catch (URISyntaxException e)
        {
            return null;
        }
        return dir;
    }
    
    public static ClassLoader getPluginClassLoader(PluginDescriptor descr)
    {
        return pluginManager.getPluginClassLoader(descr);
    }
    
    public static ClassLoader getPluginClassLoader(Object obj)
    {
        Plugin plugin = pluginManager.getPluginFor(obj);
        if (plugin != null)
            return pluginManager.getPluginClassLoader(plugin.getDescriptor());

        return obj.getClass().getClassLoader();
    }

    public static URL getResource(PluginDescriptor descr, String resourceName) 
    { 
        return getPluginClassLoader(descr).getResource(resourceName); 
    }

    //  this will lookup resource "within" plug-in the given object belongs to 
    //  the obj can be of any kind (SomeClass.class, this, this.getClass() - any!) 
    public static URL getResource(Object obj, String resourceName) 
    { 
        Plugin plugin = pluginManager.getPluginFor(obj);
        if (plugin != null)
            return getResource(plugin.getDescriptor(), resourceName);

        return obj.getClass().getClassLoader().getResource(resourceName); 
    }

    public static PathResolver getPathResolver()
    {
        return pluginManager.getPathResolver();
    } 

}
