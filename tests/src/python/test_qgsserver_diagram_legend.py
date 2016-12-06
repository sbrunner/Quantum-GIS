# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsServer.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Stephane Brunner'
__date__ = '01/03/2017'
__copyright__ = 'Copyright 2017, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

print('CTEST_FULL_OUTPUT')

import qgis  # NOQA

import os
from shutil import copyfile
from math import sqrt
from qgis.testing import unittest
from utilities import unitTestDataPath
from osgeo import gdal
from osgeo.gdalconst import GA_ReadOnly
from qgis.server import QgsServer, QgsAccessControlFilter
from qgis.core import QgsRenderChecker, QgsApplication
from qgis.PyQt.QtCore import QSize
import tempfile
import urllib.request
import urllib.parse
import urllib.error
import base64


class TestQgsServerDiagramLegend(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""
        cls._app = QgsApplication([], False)
        cls._server = QgsServer()
        cls._server.handleRequest("")
        cls._server_iface = cls._server.serverInterface()
        cls._accesscontrol = RestrictedAccessControl(cls._server_iface)
        cls._server_iface.registerAccessControl(cls._accesscontrol, 100)

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        del cls._server
        cls._app.exitQgis()

    def setUp(self):
        self.testdata_path = unitTestDataPath("qgis_server_diagramlegend")

        for k in ["QUERY_STRING", "QGIS_PROJECT_FILE"]:
            if k in os.environ:
                del os.environ[k]

        self.projectPath = os.path.join(self.testdata_path, "project.qgs")
        self.assertTrue(os.path.isfile(self.projectPath), 'Could not find project file "{}"'.format(self.projectPath))

    def test_wms__getlegendgraphicbottom(self):
        self._getlegendgraphic("Bottom")

    def test_wms__getlegendgraphictop(self):
        self._getlegendgraphic("Top")

    def test_wms__getlegendgraphicmiddle(self):
        self._getlegendgraphic("Middle")

    def test_wms__getlegendgraphicmulti(self):
        self._getlegendgraphic("Multi")

    def _getlegendgraphic(self, layer):
        query_string = "&".join(["%s=%s" % i for i in list({
            "MAP": urllib.parse.quote(self.projectPath),
            "SERVICE": "WMS",
            "VERSION": "1.1.1",
            "REQUEST": "GetLegendGraphic",
            "LAYERS": layer,
            "FORMAT": "image/png"
        }.items())])

        response, headers = self._get_(query_string)
        self._img_diff_error(response, headers, "WMS_GetLegendGraphic_" + layer, 250, QSize(10, 10))

    def _get_(self, query_string):
        self._server.putenv("REQUEST_METHOD", "GET")
        data = self._server.handleRequest(query_string)
        self._server.putenv("REQUEST_METHOD", '')

        headers = {}
        for line in data[0].decode('UTF-8').split("\n"):
            if line != "":
                header = line.split(":")
                self.assertEqual(len(header), 2, line)
                headers[str(header[0])] = str(header[1]).strip()
        return data[1], headers

    def _img_diff(self, image, control_image, max_diff, max_size_diff=QSize()):
        temp_image = os.path.join(tempfile.gettempdir(), "%s_result.png" % control_image)

        with open(temp_image, "wb") as f:
            f.write(image)

        control = QgsRenderChecker()
        control.setControlPathPrefix("qgis_server_accesscontrol")
        control.setControlName(control_image)
        control.setRenderedImage(temp_image)
        if max_size_diff.isValid():
            control.setSizeTolerance(max_size_diff.width(), max_size_diff.height())
        return control.compareImages(control_image), control.report()

    def _img_diff_error(self, response, headers, image, max_diff=10, max_size_diff=QSize()):
        self.assertEqual(
            headers.get("Content-Type"), "image/png",
            "Content type is wrong: %s" % headers.get("Content-Type"))
        test, report = self._img_diff(response, image, max_diff, max_size_diff)

        with open(os.path.join(tempfile.gettempdir(), image + "_result.png"), "rb") as rendered_file:
            encoded_rendered_file = base64.b64encode(rendered_file.read())
            message = "Image is wrong\n%s\nImage:\necho '%s' | base64 -d >%s/%s_result.png" % (
                report, encoded_rendered_file.strip(), tempfile.gettempdir(), image
            )

        with open(os.path.join(tempfile.gettempdir(), image + "_result_diff.png"), "rb") as diff_file:
            encoded_diff_file = base64.b64encode(diff_file.read())
            message += "\nDiff:\necho '%s' | base64 -d > %s/%s_result_diff.png" % (
                encoded_diff_file.strip(), tempfile.gettempdir(), image
            )

        self.assertTrue(test, message)

if __name__ == "__main__":
    unittest.main()
