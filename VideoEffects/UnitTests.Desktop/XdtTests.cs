using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Microsoft.Web.XmlTransform;
using System.Xml;
using System.IO;

namespace UnitTests.Desktop
{
    [TestClass]
    public class XdtTests
    {
        [TestMethod]
        public void CS_D_XDT_TestInstallThenUninstall()
        {
            var tests = new int[] { 0, 2, 3 };
            foreach(int i in tests)
            {
                var filenameIn = String.Format("Package{0}.appxmanifest", i);
                var filenameInstalled = String.Format("Package{0}.installed.appxmanifest", i);
                var filenameUninstalled = String.Format("Package{0}.uninstalled.appxmanifest", i);
                var filenameReference = String.Format("Package{0}.reference.appxmanifest", i);

                Console.WriteLine("Loading {0}", filenameIn);
                var xml = new XmlDocument();
                xml.Load(filenameIn);

                Console.WriteLine("Installing XML");
                using (var transform = new XmlTransformation("Package.appxmanifest.install.xdt"))
                {
                    Assert.IsTrue(transform.Apply(xml));
                }

                Console.WriteLine("Saving {0}", filenameInstalled);
                xml.Save(filenameInstalled);

                Console.WriteLine("Verifying output");
                string actual = File.ReadAllText(filenameInstalled);
                string expected = File.ReadAllText(filenameReference);
                Assert.AreEqual(expected, actual);

                Console.WriteLine("Uninstalling XML");
                using (var transform = new XmlTransformation("Package.appxmanifest.uninstall.xdt"))
                {
                    Assert.IsTrue(transform.Apply(xml));
                }

                Console.WriteLine("Saving {0}", filenameUninstalled);
                xml.Save(filenameUninstalled);

                Console.WriteLine("Verifying output");
                actual = File.ReadAllText(filenameUninstalled);
                expected = File.ReadAllText(filenameIn);
                Assert.AreEqual(expected, actual);
            }
        }

        [TestMethod]
        public void CS_D_XDT_TestUninstall()
        {
            var filenameIn = "Package4.appxmanifest";
            var filenameUninstalled = "Package4.uninstalled.appxmanifest";
            var filenameReference = "Package4.reference.appxmanifest";

            Console.WriteLine("Loading {0}", filenameIn);
            var xml = new XmlDocument();
            xml.Load(filenameIn);

            Console.WriteLine("Uninstalling XML");
            using (var transform = new XmlTransformation("Package.appxmanifest.uninstall.xdt"))
            {
                Assert.IsTrue(transform.Apply(xml));
            }

            Console.WriteLine("Saving {0}", filenameUninstalled);
            xml.Save(filenameUninstalled);

            Console.WriteLine("Verifying output");
            string actual = File.ReadAllText(filenameUninstalled);
            string expected = File.ReadAllText(filenameReference);
            Assert.AreEqual(expected, actual);
        }
    }
}
